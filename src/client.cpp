#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <atomic>
#include <thread>
#include <fstream>
#include <vector>
#include <unistd.h>

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
typedef boost::asio::ip::tcp::socket tcp_socket;

std::vector<int> durations;

class Connection : public boost::enable_shared_from_this<Connection>
{
static std::atomic<std::size_t> runningConnections;
static std::atomic<std::size_t> cancledConnections;
public:
  Connection(boost::asio::io_service &io_service,
             boost::asio::ssl::context &context)
      : SSLSocket(io_service, context), TCPSocket(io_service),
        buffer(1024)
  {
    ++runningConnections;
    connectionID = runningConnections - 1;
  }

  std::size_t getConnectionID()
  {
    return connectionID;
  }

  ~Connection()
  {
    Connection::decreaseRunningConnection();
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
    SSLSocket.set_verify_mode(boost::asio::ssl::verify_peer);
    SSLSocket.set_verify_callback(
        boost::bind(&Connection::verify_certificate, this, _1, _2));
    SSLSocket.async_handshake(boost::asio::ssl::stream_base::client,
      boost::bind(&Connection::handle_handshake, shared_from_this(),
        boost::asio::placeholders::error));
  }

  void StartRead()
  {
    TCPSocket.async_receive(boost::asio::buffer(buffer),
                             boost::bind(&Connection::HandleRead, shared_from_this(),
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));
  }


  static std::atomic<std::size_t> connectionError;
  static std::atomic<std::size_t> handshakeError;
  static std::atomic<std::size_t> writeError;
  bool failed = false;
  decltype(std::chrono::steady_clock::now()) startTime;
  decltype(std::chrono::steady_clock::now()) stopTime;
  std::size_t connectionID;


  static std::size_t getRunningConnections() { return runningConnections; }
  static std::size_t getCancledConnections() { return cancledConnections; }
  static void increaseCancledConnection() { ++cancledConnections; }
  static void decreaseRunningConnection() { --runningConnections; }

private:
  bool verify_certificate(bool preverified,
      boost::asio::ssl::verify_context& ctx)
  {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // In this example we will simply print the certificate's subject name.
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

    return preverified;
  }

  void handle_handshake(const boost::system::error_code& error)
  {

    if (!error)
    {
      SSLSocket.async_read_some(boost::asio::buffer(buffer),
                              boost::bind(&Connection::HandleRead, shared_from_this(),
                                          boost::asio::placeholders::error,
                                          boost::asio::placeholders::bytes_transferred));
    }
  }


  void HandleRead(const boost::system::error_code& error, size_t bytes_transferred)
  {
    if (!error)
    {
      std::string begin;
      for (int i = 0; i < buffer.size(); ++i)
      {
        if (isdigit(buffer[i]))
          begin.push_back(buffer[i]);
      }
      auto now = std::chrono::high_resolution_clock::now();
      auto end_time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      std::string end = std::to_string(end_time);
      int delay = (int)(std::stol(end) - std::stol(begin));
      durations[connectionID] = delay;
      printf("%s \n%s \n %lu : %d \n", begin.c_str(), end.c_str(), connectionID, delay);
    }
    else
    {
      printf("read error");
    }
  }

  std::vector<char> buffer;
  ssl_socket SSLSocket;
  tcp_socket TCPSocket;
};


std::atomic<std::size_t> Connection::runningConnections{0};
std::atomic<std::size_t> Connection::cancledConnections{0};
std::atomic<std::size_t> Connection::connectionError{0};
std::atomic<std::size_t> Connection::handshakeError{0};
std::atomic<std::size_t> Connection::writeError{0};


class ClientService
{
public:
  ClientService(boost::asio::io_service &service,
                boost::asio::ssl::context &context,
                boost::asio::ip::tcp::resolver::iterator &iterator,
                std::size_t connections,
                std::string isTLS)
                : m_service(service), m_context(context), m_iterator(iterator),
                m_connections(connections), IsTLS(isTLS)
  {
    //do nothing
  }

  void start()
  {
    for (std::size_t i = 0; i != m_connections; ++i)
    {
      auto new_connection = boost::make_shared<Connection>(m_service, m_context);

      if (IsTLS == "TCP")
      {
        boost::asio::async_connect(new_connection->GetTcpSocket(), m_iterator,
                                   boost::bind(&ClientService::handle_connect,
                                               this, new_connection,
                                               boost::asio::placeholders::error));
      }
      if (IsTLS == "TLS")
      {
        boost::asio::async_connect(new_connection->GetSSLSocket(), m_iterator,
                                   boost::bind(&ClientService::handle_connect,
                                               this, new_connection,
                                               boost::asio::placeholders::error));
      }

    }
  }

  void handle_connect(boost::shared_ptr<Connection> new_connection,
      const boost::system::error_code& error)
  {
    if (!error) {
      if (IsTLS == "TCP") {
        new_connection->StartRead();
      }
      if (IsTLS == "TLS")
      {
        new_connection->StartHandshake();
      }
    }

  }

  boost::asio::io_service &m_service;
  boost::asio::ssl::context &m_context;
  boost::asio::ip::tcp::resolver::iterator &m_iterator;
  std::size_t m_connections;
  std::string IsTLS;
};

std::vector<std::thread> createThreads(
    boost::asio::io_service &ioService, std::size_t number)
{
    std::vector<std::thread> threads;
    std::generate_n(std::back_inserter(threads), number,
        [&] { return std::thread{[&] { ioService.run(); }}; });

    return threads;
}

std::chrono::milliseconds measureTransferTime(ClientService &cService,
                                              boost::asio::io_service &service)
{

    auto startTime = std::chrono::steady_clock::now();

    cService.start();

    auto threads = createThreads(service, 4);
    for (auto &thread : threads)
        thread.join();

    while (Connection::getRunningConnections() != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    auto stopTime = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        stopTime - startTime);
}

int main(int argc, char const *argv[])
{
  try
  {
    if (argc < 5)
    {
      std::cout << "Usage: " << argv[0] << " ip port connections mode" << std::endl;
      return 1;
    }
    std::string ip = argv[1];
    std::string port = argv[2];
    std::size_t connections = std::atoll(argv[3]);
    std::string isTLS = argv[4];

    durations = std::vector<int>(connections);

    boost::asio::io_service io_service;

    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(ip, port);
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file("cacert.pem");

    ClientService cService(io_service, ctx, iterator, connections, isTLS);
    measureTransferTime(cService, io_service);

    double average = 0;
    for (auto d : durations)
    {
      average += d;
    }

    average = average / connections;

    double median;

    std::sort(durations.begin(), durations.end());

    int size = durations.size();
    if (size  % 2 == 0)
    {
        median = (durations[size / 2 - 1] + durations[size / 2]) / 2;
    }
    else
    {
        median = durations[size / 2];
    }

    std::cout << connections << " " << average << " "  << median << std::endl;

  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}