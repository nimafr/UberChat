//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <ios>
#include <iomanip>
#include <thread>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/crc.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>


#include "chat_message.hpp"
#include "utils.hpp"

using boost::asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

class chat_client
{
public:
  chat_client(boost::asio::io_service& io_service,
      tcp::resolver::iterator endpoint_iterator,void (*data_recv) (chat_message cm))
    : io_service_(io_service),
      socket_(io_service),data_recv_ ( data_recv )
  {
    do_connect(endpoint_iterator);
  }

  void write(const chat_message& msg)
  {
    io_service_.post(
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    io_service_.post([this]() { socket_.close(); });
  }

private:
  void do_connect(tcp::resolver::iterator endpoint_iterator)
  {
    boost::asio::async_connect(socket_, endpoint_iterator,
        [this](boost::system::error_code ec, tcp::resolver::iterator)
        {
          if (!ec)
          {
            do_read_header();
          }
        });
  }

  void do_read_header()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_read_body()
  {
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
           /* std::cout << "Recieved: [";
            std::cout.write(read_msg_.body(), read_msg_.body_length());
            std::cout << "]\n";*/
            data_recv_ ( read_msg_ );
            do_read_header();
          }
          else
          {
            socket_.close();
          }
        });
  }

  void do_write()
  {
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            socket_.close();
          }
        });
  }

private:
 boost::asio::io_service& io_service_;
  tcp::socket socket_;
  void (*data_recv_) (chat_message cm);
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};
#ifdef XXX
int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: chat_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    tcp::resolver resolver(io_service);
    auto endpoint_iterator = resolver.resolve({ argv[1], argv[2] });
    chat_client c(io_service, endpoint_iterator);

    std::thread t([&io_service](){ io_service.run(); });

    std::cout << "This program is used for stimulating the "
              << "UberChat server." << std::endl;
    std::cout << "It is strictly for testing and software integration" << std::endl;
    std::cout << "enter the command and any arguments." << std::endl;
    std::cout << "the time and the checksum will be added." << std::endl;
    std::cout << "enter a 'q' to exit" << std::endl << std::endl;
    bool exit_flag = false;
    char line[chat_message::max_body_length + 1] = { 0 };
    while (!exit_flag &&
            std::cin.getline(line, chat_message::max_body_length + 1))
    {
       if ( strlen(line)==1 && line[0]=='q' )
       {
          exit_flag = true;
       }
       else
       {
          // a command we want to process
          chat_message msg;
          msg = format_reply ( line );
          msg.encode_header();
          c.write(msg);
          memset ( line, '\0', sizeof ( line ) );
          sleep (1); // helps with scripted tests
       }
    }

    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
#endif
