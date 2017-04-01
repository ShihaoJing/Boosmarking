#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <atomic>
#include <thread>

class Connection : public boost::enable_shared_from_this<Connection>
{
static std::atomic<std::size_t> runningConnections;
static std::atomic<std::size_t> cancledConnections;
public:
  Connection(boost::asio::io_service &io_service,
             boost::asio::ssl::context &context, 
             std::size_t messages, std::size_t messageSize)
      : sock(io_service, context), m_messages(messages), 
        buffer(messageSize, '0')
  {
    ++runningConnections;
    connectionID = runningConnections;
    sock.set_verify_mode(boost::asio::ssl::verify_peer);
    sock.set_verify_callback(
        boost::bind(&Connection::verify_certificate, this, _1, _2));
  }

  std::size_t getConnectionID()
  {
    return connectionID;
  }

  ~Connection() { --runningConnections; }

  static std::size_t getRunningConnections() { return runningConnections; }
  static std::size_t getCancledConnections() { return cancledConnections; }
  static void increaseCancledConnection() { ++cancledConnections; }

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket>
                  ::lowest_layer_type &socket()
  {
    return sock.lowest_layer();
  }

  void start()
  {
    sock.async_handshake(boost::asio::ssl::stream_base::client,
      boost::bind(&Connection::handle_handshake, shared_from_this(),
        boost::asio::placeholders::error));
  }

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
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(sock, boost::asio::buffer(buffer),
                               boost::bind(&Connection::handle_write,
                               shared_from_this(), 
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
      }
      
    }
    else
    {
      std::cout << "# " <<  getConnectionID() << std::endl;
      std::cout << " handshake error: " << error.message() << std::endl;
      Connection::increaseCancledConnection();
    }
  }

  void handle_write(const boost::system::error_code& error,
                     std::size_t bytes_transferred)
  {
    if (!error)
    {
      if (m_messages > 0)
      {
        --m_messages;
        boost::asio::async_write(sock, boost::asio::buffer(buffer),
                               boost::bind(&Connection::handle_write,
                               shared_from_this(), 
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred));
      }
    }
    else
    {
      std::cout << "# " <<  getConnectionID() << std::endl;
      std::cout << " write error: " << error.message() << std::endl;
      Connection::increaseCancledConnection();
    }
  }

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sock;
  std::vector<char> buffer;
  std::size_t m_messages;
  std::size_t connectionID;
};

std::atomic<std::size_t> Connection::runningConnections{0};
std::atomic<std::size_t> Connection::cancledConnections{0};

class ClientService
{
public:
  ClientService(boost::asio::io_service &service,
                boost::asio::ssl::context &context,
                boost::asio::ip::tcp::resolver::iterator &iterator,
                std::size_t connections, std::size_t messages, 
                std::size_t messageSize)
                : m_service(service), m_context(context), m_iterator(iterator), 
                m_connections(connections), m_messageSize(messageSize), 
                m_messages(messages)
  {
    //do nothing
  }

  void start() 
  { 
    for (std::size_t i = 0; i != m_connections; ++i)
    {
      auto new_connection = boost::make_shared<Connection>(m_service, 
                                  m_context, m_messages, 
                                  m_messageSize);
                           
      boost::asio::async_connect(new_connection->socket(), m_iterator,
            boost::bind(&ClientService::handle_connect, this, new_connection,
            boost::asio::placeholders::error));
    } 
  }

  void handle_connect(boost::shared_ptr<Connection> new_connection,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_connection->start();
    }
    else
    {
      std::cout << "# " <<  new_connection->getConnectionID() << std::endl;
      std::cout << " connection error: " << error.message() << std::endl;
      Connection::increaseCancledConnection();
    }
  }

private:
  boost::asio::io_service &m_service;
  boost::asio::ssl::context &m_context;
  boost::asio::ip::tcp::resolver::iterator &m_iterator;
  std::size_t m_messageSize;
  std::size_t m_messages;
  std::size_t m_connections;
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
    if (argc < 6) {
      std::cout << "Usage: " << argv[0]
                << " ip port connections messages messageSize" << std::endl;

      return 1;
    }
    std::string ip = argv[1];
    std::string port = argv[2];
    std::size_t connections = std::atoll(argv[3]);
    std::size_t messages = std::atoll(argv[4]);
    std::size_t messageSize = std::atoll(argv[5]);

    boost::asio::io_service io_service;

    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(ip, port);
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file("cacert.pem");

    ClientService cService(io_service, ctx, iterator, connections, messages, messageSize);

    auto duration = measureTransferTime(cService, io_service);
    auto seconds = static_cast<double>(duration.count()) / 1000;
    auto megabytes =
        static_cast<double>((connections-static_cast<double>(Connection::getCancledConnections())) * messages * messageSize) / 1024 / 1024;

    std::cout << megabytes << " megabytes sent and received in " << seconds
              << " seconds. (" << (megabytes / seconds) << " MB/s)"
              << std::endl;
    std::cout << "Failed Connections: " << Connection::getCancledConnections() << std::endl;
    

  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}