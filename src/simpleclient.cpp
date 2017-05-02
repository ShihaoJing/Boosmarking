//
// Created by shihaojing on 4/20/17.
//
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <chrono>

using boost::asio::ip::tcp;

enum { max_length = 1024 };

int main(int argc, char* argv[])
{
  try
    {
      if (argc != 3)
      {
        std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
        return 1;
      }



      boost::asio::io_service io_service;

      tcp::socket s(io_service);
      tcp::resolver resolver(io_service);
      boost::asio::connect(s, resolver.resolve({argv[1], argv[2]}));


      std::vector<char> buffer(1024);

      size_t length = s.read_some(boost::asio::buffer(buffer));
      std::string begin(buffer.begin(), buffer.end());

      auto now = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

      std::string end = std::to_string(duration);

      std::cout << begin << std::endl;
      std::cout << end << std::endl;

      std::cout << "\n";
    }
  catch (std::exception& e)
    {
      std::cerr << "Exception: " << e.what() << "\n";
    }

  return 0;
}
