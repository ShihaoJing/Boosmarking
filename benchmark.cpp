#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/error.hpp>
#include <boost/bind.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

class ServerConnection {
public:
    ServerConnection(boost::asio::io_service &ioService, boost::asio::ssl::context &context,
        std::size_t messageSize)
        : m_socket{ioService, context}
        , m_buffer(messageSize)
    {
        ++s_runningConnections;
    }

    ~ServerConnection() { --s_runningConnections; }

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type &socket()
    {
        return m_socket.lowest_layer();
    }

    void start(std::shared_ptr<ServerConnection> self, std::size_t messages)
    {
        m_socket.async_handshake(boost::asio::ssl::stream_base::server,
            [=](const boost::system::error_code &) { asyncRead(self, messages); });
    }

    static std::size_t runningConnections() { return s_runningConnections; }

private:
    void asyncRead(std::shared_ptr<ServerConnection> self, std::size_t messages)
    {
        boost::asio::async_read(m_socket, boost::asio::buffer(m_buffer),
            [=](const boost::system::error_code &, std::size_t) {
                if (messages > 1)
                    asyncRead(self, messages - 1);
            });
    }

    static std::atomic<std::size_t> s_runningConnections;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
    std::vector<char> m_buffer;
};

std::atomic<std::size_t> ServerConnection::s_runningConnections{0};


class Server {
public:
    Server(boost::asio::io_service &ioService, std::size_t connections,
        std::size_t messages, std::size_t messageSize)
        : m_ioService{ioService}
        , m_messages{messages}
        , m_messageSize{messageSize}
    {
        m_context.use_certificate_chain_file("cacert.pem");
        m_context.use_private_key_file("privatekey.pem", boost::asio::ssl::context::pem);
        asyncAccept(connections);
    }

private:
    void asyncAccept(std::size_t connections)
    {
        auto conn = std::make_shared<ServerConnection>(
            m_ioService, m_context, m_messageSize);

        m_acceptor.async_accept(conn->socket(), [=](const boost::system::error_code &) {
            conn->start(conn, m_messages);
            if (connections > 1)
                asyncAccept(connections - 1);
        });
    }

    boost::asio::io_service &m_ioService;
    std::size_t m_messages;
    std::size_t m_messageSize;

    boost::asio::ssl::context m_context{boost::asio::ssl::context::tlsv12_server};
    boost::asio::ip::tcp::acceptor m_acceptor{
        m_ioService, {boost::asio::ip::tcp::v4(), 8080}};
};

class ClientConnection {
public:
    ClientConnection(boost::asio::io_service &ioService,
        boost::asio::ip::tcp::resolver::iterator iterator, std::size_t messageSize)
        : m_socket{ioService, m_context}
        , m_buffer(messageSize)
    {
        boost::asio::connect(m_socket.lowest_layer(), iterator);
        m_socket.handshake(boost::asio::ssl::stream_base::client);
        ++c_runningConnections;
    }

    void write_handler(
        const boost::system::error_code& ec,
        std::size_t bytes_transferred, std::size_t messages)
    {
        if (messages > 0)
        {
          asyncSend(messages - 1);
        }
        if (messages == 0)
          --c_runningConnections;
    }

    void asyncSend(std::size_t messages)
    {
        boost::asio::async_write(m_socket, boost::asio::buffer(m_buffer),
          boost::bind(&ClientConnection::write_handler,
          this, boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred, messages));
    }

    static std::size_t runningConnections() { return c_runningConnections; }


private:
    static std::atomic<std::size_t> c_runningConnections;
    boost::asio::ssl::context m_context{boost::asio::ssl::context::tlsv12_client};
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
    std::vector<char> m_buffer;
};

std::atomic<std::size_t> ClientConnection::c_runningConnections{0};

std::vector<std::thread> createThreads(
    boost::asio::io_service &ioService, std::size_t number)
{
    std::vector<std::thread> threads;
    std::generate_n(std::back_inserter(threads), number,
        [&] { return std::thread{[&] { ioService.run(); }}; });

    return threads;
}

std::vector<std::shared_ptr<ClientConnection>> createClients(
    boost::asio::io_service &ioService, std::size_t messageSize, std::size_t number)
{
    boost::asio::ip::tcp::resolver resolver{ioService};
    auto iterator = resolver.resolve({"127.0.0.1", "8080"});

    std::vector<std::shared_ptr<ClientConnection>> clients;
    std::generate_n(std::back_inserter(clients), number, [&] {
        return std::make_shared<ClientConnection>(
            ioService, iterator, messageSize);
    });

    return clients;
}

std::chrono::milliseconds measureTransferTime(
    std::vector<std::shared_ptr<ClientConnection>> &clients,
    std::size_t messages)
{
    auto startTime = std::chrono::steady_clock::now();

    for (auto &client : clients)
        client->asyncSend(messages);

    while (ClientConnection::runningConnections() != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

    auto stopTime = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(
        stopTime - startTime);
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        std::cout << "Usage: " << argv[0]
                  << " threads connections messages messageSize" << std::endl;

        return 1;
    }

    std::size_t threadsNo = std::atoll(argv[1]);
    std::size_t connections = std::atoll(argv[2]);
    std::size_t messages = std::atoll(argv[3]);
    std::size_t messageSize = std::atoll(argv[4]);

    boost::asio::io_service ioService;
    boost::asio::io_service::work idleWork{ioService};

    auto threads = createThreads(ioService, threadsNo);
    //Server server{ioService, connections, messages, messageSize};
    auto clients = createClients(ioService, messageSize, connections);

    auto duration = measureTransferTime(clients, messages);
    auto seconds = static_cast<double>(duration.count()) / 1000;
    auto megabytes =
        static_cast<double>(connections * messages * messageSize) / 1024 / 1024;

    std::cout << megabytes << " megabytes sent and received in " << seconds
              << " seconds. (" << (megabytes / seconds) << " MB/s)"
              << std::endl;

    ioService.stop();
    for (auto &thread : threads)
        thread.join();

    return 0;
}