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

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)


std::vector<long long> durations;

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
typedef boost::asio::ip::tcp::socket tcp_socket;


class Connection : public boost::enable_shared_from_this<Connection>
{
  static std::atomic<std::size_t> runningConnections;
  static std::atomic<std::size_t> cancledConnections;
public:
  Connection(boost::asio::io_service &io_service,
             boost::asio::ssl::context &context,
             std::size_t messages, std::size_t messageSize)
      : SSLSocket(io_service, context),
        TCPSocket(io_service),
        m_messages(messages),
        buffer(messageSize, '0')
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
    stopTime = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            stopTime - startTime).count();
    durations[connectionID] = duration;
    Connection::decreaseRunningConnection();
  }

  static std::size_t getRunningConnections() { return runningConnections; }
  static std::size_t getCancledConnections() { return cancledConnections; }
  static void increaseCancledConnection() { ++cancledConnections; }
  static void decreaseRunningConnection() { --runningConnections; }

  ssl_socket::lowest_layer_type& GetSSLSocket()
  {
    return SSLSocket.lowest_layer();
  }

  tcp_socket& GetTcpSocket()
  {
    return TCPSocket;
  }

  void StartHandshkae()
  {
    SSLSocket.set_verify_mode(boost::asio::ssl::verify_peer);
    SSLSocket.set_verify_callback(
        boost::bind(&Connection::verify_certificate, this, _1, _2));
    SSLSocket.async_handshake(boost::asio::ssl::stream_base::client,
                              boost::bind(&Connection::handle_handshake,
                                          shared_from_this(),
                                          boost::asio::placeholders::error));

  }

  void StartRead()
  {
    TCPSocket.async_read_some(boost::asio::buffer(buffer),
                              boost::bind(&Connection::HandleTcpRead,
                                          shared_from_this(),
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
                             boost::bind(&Connection::HandleSSLRead,
                                         shared_from_this(),
                                         boost::asio::placeholders::error,
                                         boost::asio::placeholders::bytes_transferred));


      }
    }


  void HandleSSLRead(const boost::system::error_code& error,
                   size_t bytes_transferred)
  {
    if (!error)
    {
      --m_messages;
      if (m_messages > 0)
      {
        SSLSocket.async_read_some(boost::asio::buffer(buffer),
                                  boost::bind(&Connection::HandleSSLRead,
                                              shared_from_this(),
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
      }
    }
  }

  void HandleTcpRead(const boost::system::error_code& error,
                     size_t bytes_transferred)
  {
    if (!error)
    {
      --m_messages;
      if (m_messages > 0)
      {
        TCPSocket.async_read_some(boost::asio::buffer(buffer),
                                  boost::bind(&Connection::HandleTcpRead,
                                              shared_from_this(),
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
      }
    }
  }



  ssl_socket SSLSocket;
  tcp_socket TCPSocket;
  std::vector<char> buffer;
  std::size_t m_messages;
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
                  std::size_t connections, std::size_t messages,
                  std::size_t messageSize, std::string isTlS)
            : m_service(service), m_context(context), m_iterator(iterator),
              m_connections(connections), m_messageSize(messageSize),
              m_messages(messages), sessionMode(isTlS)
    {
        //do nothing
    }

    void start()
    {
      for (std::size_t i = 0; i != m_connections; ++i)
      {
        auto new_connection = boost::make_shared<Connection>(m_service,
                                                             m_context,
                                                             m_messages,
                                                             m_messageSize);
        new_connection->startTime = std::chrono::steady_clock::now();

        if (sessionMode == "TCP")
        {
          boost::asio::async_connect(new_connection->GetTcpSocket(), m_iterator,
                                     boost::bind(&ClientService::handle_connect,
                                                 this, new_connection,
                                                 boost::asio::placeholders::error));
        }
        if (sessionMode == "TLS")
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
      if (!error)
      {
        if (sessionMode == "TCP") {
          new_connection->StartRead();
        }
        if (sessionMode == "TLS")
        {
          new_connection->StartHandshkae();
        }
      }
    }

  boost::asio::io_service &m_service;
  boost::asio::ssl::context &m_context;
  boost::asio::ip::tcp::resolver::iterator &m_iterator;
  std::size_t m_messageSize;
  std::size_t m_messages;
  std::size_t m_connections;
  std::string sessionMode;
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
    //m_service->run();
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
      if (argc < 6)
      {
          std::cout << "Usage: " << argv[0]
                    << " ip port connections FileSize Mode" << std::endl;

          return 1;
      }
      std::string ip = argv[1];
      std::string port = argv[2];
      std::size_t connections = std::atoll(argv[3]);
      std::size_t FileSize = std::atoll(argv[4]);
      std::string isTLS = argv[5];
      std::size_t messageSize = 1024;
      std::size_t messages = Kilobytes(FileSize) / messageSize;
      durations = std::vector<long long>(connections);

      boost::asio::io_service io_service;

      boost::asio::ip::tcp::resolver resolver(io_service);
      boost::asio::ip::tcp::resolver::query query(ip, port);
      boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

      boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
      ctx.load_verify_file("cacert.pem");

      ClientService cService(io_service, ctx, iterator, connections, messages, messageSize, isTLS);

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