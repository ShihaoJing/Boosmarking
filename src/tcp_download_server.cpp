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
typedef boost::asio::ip::tcp::socket tcp_socket;

class session : public boost::enable_shared_from_this<session>
{
 public:
  session(boost::asio::io_service& io_service,
          boost::asio::ssl::context& context,
          std::size_t messages, std::size_t messageSize)
      : SSLSocket(io_service, context),
        TCPSocket(io_service),
        buffer(messageSize, 0), m_messages(messages)
  {
  }

  ssl_socket::lowest_layer_type& GetSSLSocket()
  {
    return SSLSocket.lowest_layer();
  }

  tcp_socket& GetTcpSocket()
  {
    return TCPSocket;
  }

  void StartHandshake()
  {
    SSLSocket.async_handshake(boost::asio::ssl::stream_base::server,
                              boost::bind(&session::handle_handshake,
                                          shared_from_this(),
                                          boost::asio::placeholders::error));
  }

  void StartWrite()
  {
    if (m_messages > 0)
    {
      --m_messages;
      boost::asio::async_write(TCPSocket, boost::asio::buffer(buffer),
                               boost::bind(&session::HandleTcpWrite,
                                           shared_from_this(),
                                           boost::asio::placeholders::error,
                                           boost::asio::placeholders::bytes_transferred));
    }
  }

  void HandleTcpWrite(const boost::system::error_code& error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(TCPSocket, boost::asio::buffer(buffer),
                                 boost::bind(&session::HandleTcpWrite,
                                             shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
      }
    }
  }

  void handle_handshake(const boost::system::error_code& error)
  {
    if (!error)
    {
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(SSLSocket, boost::asio::buffer(buffer),
                               boost::bind(&session::HandleSSLWrite,
                               shared_from_this(),
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
      }
    }
  }

  void HandleSSLWrite(const boost::system::error_code& error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(SSLSocket, boost::asio::buffer(buffer),
                                 boost::bind(&session::HandleSSLWrite,
                                             shared_from_this(),
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
      }
    }
  }

 private:
  ssl_socket SSLSocket;
  tcp_socket TCPSocket;
  std::vector<char> buffer;
  std::size_t m_messages;
};

class tcp_echo_server
{
 public:
  tcp_echo_server(boost::asio::io_service& io_service,
                  unsigned short port,
                  std::size_t messages,
                  std::size_t messageSize,
                  std::string mode)
      : io_service_(io_service),
        acceptor_(io_service,
                  boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),
                                                 port)),
        context_(boost::asio::ssl::context::sslv23),
        m_messages(messages), m_messageSize(messageSize),
        sessionMode(mode)

  {
    context_.set_options(
        boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::tlsv12_server);
    context_.use_certificate_chain_file("cacert.pem");
    context_.use_private_key_file("privatekey.pem", boost::asio::ssl::context::pem);

    start_accept();
  }

  void start_accept()
  {
    auto new_session = boost::make_shared<session>(io_service_,
                                                   context_, m_messages,
                                                   m_messageSize);
    if (sessionMode == "TLS")
    {
      acceptor_.async_accept(new_session->GetSSLSocket(),
                             boost::bind(&tcp_echo_server::handle_accept,
                                         this,
                                         new_session,
                                         boost::asio::placeholders::error));
    }
    if (sessionMode == "TCP")
    {
      acceptor_.async_accept(new_session->GetTcpSocket(),
                             boost::bind(&tcp_echo_server::handle_accept,
                                         this,
                                         new_session,
                                         boost::asio::placeholders::error));
    }
  }

  void handle_accept(boost::shared_ptr<session> new_session,
                     const boost::system::error_code& error)
  {
    if (!error)
    {
      if (sessionMode == "TLS")
      {
        new_session->StartHandshake();
      }

      if (sessionMode == "TCP")
      {
        new_session->StartWrite();
      }
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
  std::string sessionMode;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 4)
    {
      std::cerr << "Usage: tcp_echo_server <port> <FileSize> <Mode>\n";
      return 1;
    }

    boost::asio::io_service io_service;
    int port = atoi(argv[1]);
    std::size_t FileSize = std::atoll(argv[2]);
    std::string isTLS = argv[3];
    std::size_t messageSize = 1024;
    std::size_t messages = Kilobytes(FileSize) / messageSize;
    std::cout << messages << std::endl;

    using namespace std; // For atoi.
    tcp_echo_server s(io_service, port, messages, messageSize, isTLS);

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}