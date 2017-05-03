#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class session : public boost::enable_shared_from_this<session>
{
 public:
  session(boost::asio::io_service& io_service,
          boost::asio::ssl::context& context,
          std::size_t messages, std::size_t messageSize)
      : socket_(io_service, context), buffer(messageSize, 0), m_messages(messages)
  {
  }

  ssl_socket::lowest_layer_type& socket()
  {
    return socket_.lowest_layer();
  }

  void start()
  {
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            boost::bind(&session::handle_handshake, shared_from_this(),
                                        boost::asio::placeholders::error));
  }

  void handle_handshake(const boost::system::error_code& error)
  {
    if (!error)
    {
      socket_.async_read_some(boost::asio::buffer(buffer),
                              boost::bind(&session::handle_read, shared_from_this(),
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
      /*if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(socket_, boost::asio::buffer(buffer),
                               boost::bind(&session::handle_write,
                               shared_from_this(),
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
      }*/
    }
    else
    {
    }
  }

  void handle_read(const boost::system::error_code& error,
                   size_t bytes_transferred)
  {
    if (!error)
    {
      socket_.async_read_some(boost::asio::buffer(buffer),
                              boost::bind(&session::handle_read, shared_from_this(),
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    }
  }

  void handle_write(const boost::system::error_code& error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      //std::cout << m_messages << std::endl;
      /*socket_.async_read_some(boost::asio::buffer(data_, max_length),
          boost::bind(&session::handle_read, this,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));*/
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(socket_, boost::asio::buffer(buffer),
                                 boost::bind(&session::handle_write,
                                             shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
      }
    }
    else
    {
    }
  }

 private:
  ssl_socket socket_;
  std::vector<char> buffer;
  std::size_t m_messages;
};

class tcp_echo_server
{
 public:
  tcp_echo_server(boost::asio::io_service& io_service, unsigned short port, std::size_t messages, std::size_t messageSize)
      : io_service_(io_service),
        acceptor_(io_service,
                  boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
        context_(boost::asio::ssl::context::sslv23),
        m_messages(messages), m_messageSize(messageSize)

  {
    context_.set_options(
        boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2);
    context_.use_certificate_chain_file("cacert.pem");
    context_.use_private_key_file("privatekey.pem", boost::asio::ssl::context::pem);

    start_accept();
  }

  void start_accept()
  {
    auto new_session = boost::make_shared<session>(io_service_,
                                                   context_, m_messages,
                                                   m_messageSize);
    acceptor_.async_accept(new_session->socket(),
                           boost::bind(&tcp_echo_server::handle_accept, this, new_session,
                                       boost::asio::placeholders::error));
  }

  void handle_accept(boost::shared_ptr<session> new_session,
                     const boost::system::error_code& error)
  {
    if (!error)
    {
      new_session->start();
    }
    else
    {
    }

    start_accept();
  }

 private:
  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ssl::context context_;
  std::size_t m_messageSize;
  std::size_t m_messages;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc <= 2)
    {
      std::cerr << "Usage: tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;
    std::size_t FileSize = std::atoll(argv[2]);
    std::size_t messageSize = 4096;
    std::size_t messages = Megabytes(FileSize) / messageSize;
    std::cout << messages << std::endl;

    using namespace std; // For atoi.
    tcp_echo_server s(io_service, atoi(argv[1]), messages, messageSize);

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}