//
// Created by shihaojing on 4/20/17.
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <chrono>
#include <thread>

using boost::asio::ip::tcp;

const int max_length = 1024;

void session(tcp::socket sock)
{
  try
  {
      auto now = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
      printf("%ld \n", duration);

      std::string time = std::to_string(duration);

      std::vector<char> buffer(time.size());

      for (char c: time)
      {
        buffer.push_back(c);
      }


      boost::asio::write(sock, boost::asio::buffer(buffer));

  }
  catch (std::exception& e)
  {
    std::cerr << "Exception in thread: " << e.what() << "\n";
  }
}

void server(boost::asio::io_service& io_service, unsigned short port)
{
  tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
  for (;;)
  {
    tcp::socket sock(io_service);
    a.accept(sock);
    std::thread(session, std::move(sock)).detach();
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    server(io_service, std::atoi(argv[1]));
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}