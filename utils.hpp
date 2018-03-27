chat_message  format_reply ( std::string reply  );
bool  parse_command ( chat_message m, 
                      boost::crc_32_type *checksum,
                      boost::posix_time::ptime *time,
                      std::string *command,
                      std::string *payload );
