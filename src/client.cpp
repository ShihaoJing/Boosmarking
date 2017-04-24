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


std::vector<long long> durations;

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

  ~Connection()
  {
    
  }

  static std::size_t getRunningConnections() { return runningConnections; }
  static std::size_t getCancledConnections() { return cancledConnections; }
  static void increaseCancledConnection() { ++cancledConnections; }
  static void decreaseRunningConnection() { --runningConnections; }

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket>
                  ::lowest_layer_type &socket()
  {
    return sock.lowest_layer();
  }

  void start()
  {
    sleep(100);
    //boost::asio::read(sock, boost::asio::buffer(buffer));
    // boost::asio::async_read(sock, boost::asio::buffer(buffer),
    //       boost::bind(&Connection::handle_read, shared_from_this(),
    //         boost::asio::placeholders::error,
    //         boost::asio::placeholders::bytes_transferred));
    /*sock.async_handshake(boost::asio::ssl::stream_base::client,
      boost::bind(&Connection::handle_handshake, shared_from_this(),
        boost::asio::placeholders::error));*/
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
      // std::cout << "# " <<  getConnectionID() << std::endl;
      // std::cout << "handshake error: " << error.message() << std::endl;
      ++handshakeError;
      failed = true;
      increaseCancledConnection();
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
      // std::cout << "# " <<  getConnectionID() << std::endl;
      // std::cout << "write error: " << error.message() << std::endl;
      ++writeError;
      failed = true;
      increaseCancledConnection();
    }
  }

  void handle_read(const boost::system::error_code& error,
      size_t bytes_transferred)
  {
    if (!error)
    {
      sock.async_read_some(boost::asio::buffer(buffer),
          boost::bind(&Connection::handle_read, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
    else
    {
      delete this;
    }
  }

  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sock;
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
      new_connection->startTime = std::chrono::steady_clock::now();
      //auto duration = new_connection->startTime.time_since_epoch();
      //auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
      //std::cout << "ID : " << new_connection->connectionID << " : " << milliseconds_since_epoch << std::endl;

      //printf("%lu %lld \n", new_connection->connectionID, millis);
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
      new_connection->stopTime = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                      new_connection->stopTime - new_connection->startTime).count();
      auto seconds = static_cast<double>(duration / 1000);
      printf("%lu %lld \n", new_connection->connectionID, duration);
      durations[new_connection->connectionID] = duration;
      if (new_connection->failed)
      {
        durations[new_connection->connectionID] = -1.0;
      }
      Connection::decreaseRunningConnection();
      
      if (Connection::getRunningConnections() == 0)
      {
        double average = 0;
        for (auto d : durations)
        {
          if (d > -1.0)
          {
            //std::cout << d << std::endl;
            average += d;
          }
        }
        average = average / m_connections;
        std::cout << "average: " << average << std::endl;
      }
      new_connection->start();

    }
    else
    {
      // std::cout << "# " <<  new_connection->getConnectionID() << std::endl;
      // std::cout << "connection error: " << error.message() << std::endl;
      new_connection->failed = true;
      ++Connection::connectionError;
      Connection::increaseCancledConnection();
    }
  }

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

    auto threads = createThreads(service, cService.m_connections);
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
    //if (argc < 6) {
    if (argc < 4)
    {
      std::cout << "Usage: " << argv[0]
                //<< " ip port connections messages messageSize" << std::endl;
                  << " ip port connections" << std::endl;

      return 1;
    }
    std::string ip = argv[1];
    std::string port = argv[2];
    std::size_t connections = std::atoll(argv[3]);
    //std::size_t messages = std::atoll(argv[4]);
    //std::size_t messageSize = std::atoll(argv[5]);
    std::size_t messages = 1;
    std::size_t messageSize = 1;
    durations = std::vector<long long>(connections+1);

    boost::asio::io_service io_service;

    boost::asio::ip::tcp::resolver resolver(io_service);
    boost::asio::ip::tcp::resolver::query query(ip, port);
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    ctx.load_verify_file("cacert.pem");

    //ClientService cService(io_service, ctx, iterator, connections, messages, messageSize);
    ClientService cService(io_service, ctx, iterator, connections, 1, 1);

    auto duration = measureTransferTime(cService, io_service);
    auto seconds = static_cast<double>(duration.count()) / 1000;
    auto megabytes =
        static_cast<double>((connections-static_cast<double>(Connection::getCancledConnections())) * messages * messageSize) / 1024 / 1024;

     std::cout << "success: " << connections << " fail:" << Connection::getCancledConnections()
               << std::endl;

    for (int i = 1; i < connections+1; ++i)
    {
      printf("%d %llu\n", i, durations[i]);
    }

    double average = 0;
    for (auto d : durations)
    {
      if (d > -1.0)
      {
        //std::cout << d << std::endl;
        average += d;
      }
    }

    average = average / (connections-static_cast<double>(Connection::getCancledConnections()));

    double median;

    std::sort(durations.begin()+1, durations.end());
    int startPos = 1;
    for (int i = 0; i < durations.size(); ++i)
    {
      if (durations[i] > 0.0)
      {
        startPos = i;
        break;
      }
    }

    int size = (durations.size() - startPos);
    if (size  % 2 == 0)
    {
        median = (durations[startPos + size / 2 - 1] + durations[startPos + size / 2]) / 2;
    }
    else
    {
        median = durations[startPos + size / 2];
    }

    std::cout << connections << " " << average << " "  << median << std::endl;

    // std::cout << "Total connections: " << connections << std::endl;
    // std::cout << "Failed Connections: " << Connection::getCancledConnections() << std::endl;
    // std::cout << "Connection errors: " << Connection::connectionError << std::endl;
    // std::cout << "Handshake errors: " << Connection::handshakeError << std::endl;
    // std::cout << "Write errors: " << Connection::writeError << std::endl;
    // std::cout << megabytes << " megabytes sent and received in " << seconds
    //           << " seconds. (" << (megabytes / seconds) << " MB/s)"
    //           << std::endl;

    // if (argc == 7)
    // {
    //   std::cout << argv[6] << std::endl;
    //   std::ofstream out(argv[6], std::ofstream::out|std::ofstream::app);
    //   if (out)
    //   {
    //     out << connections << " " << Connection::getCancledConnections() << " "
    //       << Connection::connectionError << " " << Connection::handshakeError
    //       << " " << Connection::writeError << " " << megabytes << " "
    //       << seconds << std::endl;
    //     out.close();
    //   }
    //   else
    //   {
    //     std::cerr << "can not open " << argv[6] << std::endl;
    //   }

    // }

  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}