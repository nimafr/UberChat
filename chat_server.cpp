//
// chat_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include <sstream>
#include <ios>
#include <iomanip>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/crc.hpp>


#include "chat_message.hpp"
#include "utils.hpp"

using boost::asio::ip::tcp;

//----------------------------------------------------------------------
typedef std::deque<chat_message> chat_message_queue;


typedef std::vector<std::string> chat_history_t;
//----------------------------------------------------------------------
//  this class holds all of the data to support the clients.
//  it is the 'chat database'
//

struct nick_and_chatroom
{
   std::string nick;
   std::string chatroom;
};
// this data is also available in the participant classes,
// but to use it the classes would have to be locked.


boost::mutex db_lock;
// this is a very 'coarse' lock.
// grab it before using anything in the chat database

typedef std::map<boost::uuids::uuid,nick_and_chatroom> uuid_to_nick_t;
typedef std::map<boost::uuids::uuid,nick_and_chatroom>::iterator uuid_to_nick_itr_t;

class chat_db
{
 public:
  uuid_to_nick_t uuid_to_nick;
  std::vector<std::string> chatrooms;
  std::map<std::string,chat_history_t> chat_history;
} DB;

//----------------------------------------------------------------------
class chat_participant
{
public:
  chat_participant ()
  {
     UUID = boost::uuids::random_generator () ();
     CHATROOM = "the lobby";
     CHATROOM_MESSAGE_IDX = 0;
     std::cout << "participant () " << UUID << std::endl;
  }

  ~chat_participant()
  {
     std::cout << "~participant () " << UUID << std::endl;
  }

  virtual void deliver(const chat_message& msg) = 0;

  void process_command  (chat_message cm)
  {
    std::string command;
    try
    {
       boost::crc_32_type checksum;
       boost::posix_time::ptime time;
       std::string payload;
       command = "";
       bool cksum_ok = parse_command ( cm, &checksum, &time, &command, &payload );
       boost::posix_time::ptime now_time = boost::posix_time::microsec_clock::local_time ();
       boost::posix_time::time_duration dt = now_time - time;
       std::cout << "     delta time  : "
                 << boost::posix_time::to_simple_string ( dt )
                 << std::endl;

       char *ignore_cksum = getenv("IGNORE_CHECKSUM");
       if ( cksum_ok || ignore_cksum )
       {
         if (command == "REQCHATROOM")
         {
            std::cout << "     command is REQCHATROOM" << std::endl;
            chat_message reply;
            std::string results;
            results = CHATROOM;
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "REQUUID")
         {
            std::cout << "     command is REQUUID" << std::endl;
            chat_message reply;
            std::string results;
            results = to_string (UUID);
            std::cout << " answer " <<results<<std::endl;
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "NICK")
         {
            std::cout << "     command is NICK" << std::endl;
            chat_message reply;
            std::string results;
            // the server is allowed to revise the nick
            // if needed. like to fix duplicates
            results =  payload + "_";
            NICK = results;
            db_lock.lock ();
            {
              nick_and_chatroom nc;
              nc.nick = NICK;
              nc.chatroom = CHATROOM;
              DB.uuid_to_nick [ UUID ] = nc;
            }
            db_lock.unlock ();
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "CHANGECHATROOM")
         {
            std::cout << "     command is CHANGECHATROOM" << std::endl;
            chat_message reply;
            std::string results;

            bool found = false;
            unsigned int found_idx = 0;
            db_lock.lock ();
            {
              for (unsigned int i=0;i<DB.chatrooms.size ();i++)
              {
                  if ( DB.chatrooms[i] == payload )
                  {
                     found_idx = i;
                     found = true;
                  }
              }

              if ( found )
              {
                  CHATROOM_MESSAGE_IDX = 0;
                  CHATROOM = DB.chatrooms[found_idx];
                  nick_and_chatroom nc;
                  nc.nick = NICK;
                  nc.chatroom = CHATROOM;
                  DB.uuid_to_nick [ UUID ] = nc;
              }
              else
              {
                   std::cout << "     could not change chatroom "
                             << payload << " was not found" << std::endl;
              }
            }
            db_lock.unlock ();
            results =  CHATROOM;
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "REQCHATROOMS")
         {
            std::cout << "     command is REQCHATROOMS" << std::endl;
            chat_message reply;
            std::string results;
            db_lock.lock ();
            for (unsigned int  i = 0;
                 i < DB.chatrooms.size ();
                 i++)
            {
                if ( i != 0  && DB.chatrooms.size () > 1)
                {
                   results = results + ";";
                }
                results = results + DB.chatrooms [ i ];
            }
            db_lock.unlock ();

            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "NAMECHATROOM")
         {
            std::cout << "     command is NAMECHATROOM" << std::endl;
            chat_message reply;
            std::string results;
            results =  payload;
            db_lock.lock ();
            bool found = false;
            // don't add one if it already exists
            for (unsigned int i=0;i<DB.chatrooms.size ();i++)
            {
               if ( DB.chatrooms[i] == payload)
               {
                 found = true;
               }
            }

            if (!found)
            {
               DB.chatrooms.push_back ( results );
            }
            db_lock.unlock ();
            reply = format_reply ( command + "," + results );
            deliver ( reply );

         }
         else if (command == "SENDTEXT")
         {
            std::cout << "     command is SENDTEXT" << std::endl;
            chat_message reply;
            std::string results;
            db_lock.lock ();
            DB.chat_history[CHATROOM].push_back ( boost::uuids::to_string(UUID) + " " + payload );
            //DB.chat_history[CHATROOM].push_back ( NICK + ": " + payload );
            db_lock.unlock ();
            results = std::to_string ( payload.size () );
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "REQTEXT")
         {
            std::cout << "     command is REQTEXT" << std::endl;
            chat_message reply;
            std::string results;
            db_lock.lock ();
            for (unsigned int i=CHATROOM_MESSAGE_IDX;i<DB.chat_history[CHATROOM].size ();i++)
            {
               if (i != CHATROOM_MESSAGE_IDX)
               {
                   results = results + ";";
               }
               results = results + DB.chat_history[CHATROOM][i];
            }
            CHATROOM_MESSAGE_IDX = DB.chat_history[CHATROOM].size ();
            db_lock.unlock ();
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "REQUSERS")
         {
            std::cout << "     command is REQUSERS" << std::endl;
            chat_message reply;
            std::string results;
            db_lock.lock ();
            {
              // the map of vectors is not a perfect data structure.
              // the only way to see how many users are in one chatroom
              // is to go over all the users "uuid_to_nick" and count them up
              unsigned int count = 0;
              for (uuid_to_nick_itr_t i = DB.uuid_to_nick.begin();
                   i != DB.uuid_to_nick.end ();
                   i++)
              {
                 if ( i->second.chatroom == CHATROOM )
                 {
                    count++;
                 }
              }
              // with the count, we can build the request string
              unsigned int num_found = 0;
              for (uuid_to_nick_itr_t i = DB.uuid_to_nick.begin();
                   i != DB.uuid_to_nick.end ();
                   i++)
              {
                 if ( i->second.chatroom == CHATROOM )
                 {
                    num_found++;
                    if (num_found > 1 && count  > 1)
                    {
                       results = results + ";";
                    }
                    results = results + to_string ( i->first ) + "," + i->second.nick ;
                 }
              }
            }
            db_lock.unlock ();
            reply = format_reply ( command + "," + results );
            deliver ( reply );
         }
         else if (command == "GETNICK")
              {
            chat_message reply;
            std::string results;
            db_lock.lock ();
            {
                  nick_and_chatroom nc;
                 nc = DB.uuid_to_nick [ UUID ];
                 results = nc.nick;
            }
                db_lock.unlock ();
                reply = format_reply ( command + "," + results);
                deliver ( reply );

              }
         else
         {
            std::cout << command << " is not a valid command" << std::endl;
         }
       }

       //std::cout << cm.body() << std::endl;
    }
    catch (std::exception& e)
    {
       std::cout << "the command [" << command << "] was not parsed correctly" << std::endl;
       std::cerr << "Exception: " << e.what() << "\n";
    }

  }
  boost::uuids::uuid UUID;
  std::string NICK;
  std::string CHATROOM;
  unsigned int CHATROOM_MESSAGE_IDX;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket)
    : socket_(std::move(socket))
  {
     std::cout << "chat_session () " << std::endl;
  }
  ~chat_session ()
  {
     std::cout << "~chat_session () " << std::endl;
  }

  void start()
  {
    // room_.join(shared_from_this());
    std::cout << "join" << '\n';
    do_read_header();
  }

  void deliver(const chat_message& msg)
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      do_write();
    }
  }

private:
  void do_read_header()
  {
    auto self(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.data(), chat_message::header_length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec && read_msg_.decode_header())
          {
            do_read_body();
          }
          else
          {
            //room_.leave(shared_from_this());
            std::cout << "leave" << std::endl;
            std::cout << "deleting " << self->UUID << std::endl;
            db_lock.lock ();
            DB.uuid_to_nick.erase (self->UUID);
            db_lock.unlock ();
          }
        });
  }

  void do_read_body()
  {
    auto self(shared_from_this());
    boost::asio::async_read(socket_,
        boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            // room_.deliver(read_msg_);
            // temp.  this is an echo
            //deliver(read_msg_);
   self->process_command ( read_msg_ );
            //std::cout << "deliver " <<  self->UUID << "  " << std::endl;
            do_read_header();
          }
          else
          {
            //room_.leave(shared_from_this());
            std::cout << "leave " << std::endl;
          }
        });
  }

  void do_write()
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
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
            std::cout << "leave here" << std::endl;
            //room_.leave(shared_from_this());
          }
        });
  }

  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};

//----------------------------------------------------------------------

class chat_server
{
public:
  chat_server(boost::asio::io_service& io_service,
      const tcp::endpoint& endpoint)
    : acceptor_(io_service, endpoint),
      socket_(io_service)
  {
    db_lock.lock ();
    DB.chatrooms.push_back ( "the lobby" );
    db_lock.unlock ();
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(socket_,
        [this](boost::system::error_code ec)
        {
          if (!ec)
          {
            std::make_shared<chat_session>(std::move(socket_))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
  tcp::socket socket_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }

    boost::asio::io_service io_service;

    std::list<chat_server> servers;
    for (int i = 1; i < argc; ++i)
    {
      tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
      servers.emplace_back(io_service, endpoint);
    }

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

