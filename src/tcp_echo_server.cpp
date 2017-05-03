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
          boost::asio::ssl::context& context)
    : SSLSocket(io_service, context), TCPSocket(io_service)
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
        boost::bind(&session::handle_handshake, shared_from_this(),
          boost::asio::placeholders::error));
  }

  void StartEcho()
  {
    auto now = std::chrono::steady_clock::now();
    auto begin_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::string begin_str = std::to_string(begin_time);
    std::vector<char> buffer(begin_str.begin(), begin_str.end());
    boost::asio::async_write(TCPSocket, boost::asio::buffer(buffer),
                             boost::bind(&session::handle_write,
                                         shared_from_this(),
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));
  }

  void handle_handshake(const boost::system::error_code& error)
  {
    if (!error)
    {

    }
  }


  void handle_write(const boost::system::error_code& error, std::size_t bytes_transferred)
  {
    if (!error)
    {
      std::cout << bytes_transferred << " bytes transfered" << std::endl;
    }
  }

private:
  ssl_socket SSLSocket;
  tcp_socket TCPSocket;
};

class tcp_echo_server
{
public:
  tcp_echo_server(boost::asio::io_service& io_service, unsigned short port, std::string isTLS)
    : io_service_(io_service),
      acceptor_(io_service,
          boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      context_(boost::asio::ssl::context::sslv23),
      IsTLS(isTLS)

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
                                                   context_);
    if (IsTLS == "TCP")
    {
      acceptor_.async_accept(new_session->GetTcpSocket(),
                             boost::bind(&tcp_echo_server::handle_accept, this, new_session,
                                         boost::asio::placeholders::error));
    }

  }

  void handle_accept(boost::shared_ptr<session> new_session,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      if (IsTLS == "TCP")
      {
        new_session->StartEcho();
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
  std::string IsTLS;
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
    int port = std::atoi(argv[1]);
    std::string isTLS = argv[2];
    tcp_echo_server server(io_service, port, isTLS);

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}