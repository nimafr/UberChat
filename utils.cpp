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
#include <iostream>
#include <sstream>
#include <ios>
#include <iomanip>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/crc.hpp>

#include "chat_message.hpp"

bool  parse_command ( chat_message m,        
                      boost::crc_32_type *checksum,
                      boost::posix_time::ptime *time,
                      std::string *command,
                      std::string *payload )
{
   // ensure the message body is terminated with a null
   *(m.body () + m.body_length ()) = '\0';

   std::cout << "Received: [" << m.body ()  << "]" << " " << m.body_length ()<< std::endl;
   std::string cksum_string = std::string ( m.body (), 9 );
   std::string time_string = std::string ( m.body () + 9, 22 );

   
   checksum->process_bytes ( m.body () + 8, m.body_length () - 8 + 1 );
   *time = boost::posix_time::from_iso_string ( time_string );

   bool checksum_ok = false;
   
   if ( std::stoul ( cksum_string,0,16) == checksum->checksum () )
   {
       checksum_ok = true;
   }
   else
   {
       checksum_ok = false;
       std::cout << "     Received checksum " << std::hex << checksum->checksum () << std::dec
                 <<       " Calculated checksum " << cksum_string << std::endl;
   } 
   std::cout << "     checksum OK: " << checksum_ok << std::endl;


   char *cmd_ptr = strchr ( m.body () + 31, ',' );
   if (cmd_ptr)
   {
      cmd_ptr++; // skip the ,
      char *argptr = strchr ( cmd_ptr, ',' );
      if (argptr)
      {
         argptr++;
         *payload = std::string ( argptr );
         *command = std::string ( cmd_ptr, argptr - cmd_ptr - 1 );
      }
      else
      {
          *payload = std::string ("");
          *command = std::string ( cmd_ptr );
      }
   }

   return checksum_ok;
}

chat_message  format_reply ( std::string reply  )
{
   // formats a chat message, ready to send
   chat_message msg;
   boost::crc_32_type cksum;
   ssize_t line_size = reply.size() + 49;
   memset ( msg.body () , '\0', chat_message::max_body_length );
   boost::posix_time::ptime time;
   time = boost::posix_time::microsec_clock::local_time (); 
   line_size = snprintf( msg.body (), 
             reply.size () + 49,
             "%08x,%s,%s",
	         cksum.checksum (),
	         boost::posix_time::to_iso_string (time).c_str(),
	         reply.c_str() );
   // calculate the checksum
   cksum.process_bytes ( msg.body () + 8, line_size - 8  + 1);


   // store it in the message
   snprintf( msg.body (), reply.size () + 49,
             "%08x,%s,%s",
	          cksum.checksum (),
	          boost::posix_time::to_iso_string (time).c_str(),
	          reply.c_str() );
   msg.body_length ( line_size );
   std::cout << "Sending:  [" << msg.body () << "]" << " " << msg.body_length () << std::endl;
   msg.encode_header ();
   return msg;
}
