CPPFLAGS = -Wall -O0 -g -std=c++11

all: chat_server  client_gui 

chat_server: chat_server.cpp chat_message.hpp utils.cpp utils.hpp
		g++ ${CPPFLAGS} -o chat_server chat_server.cpp utils.cpp \
			-lboost_system -lboost_date_time 

client_gui: client_gui.cpp client_cli.cpp utils.cpp utils.hpp
		g++ ${CPPFLAGS} -o client_gui client_gui.cpp utils.cpp \
			-lboost_system -lboost_date_time -lpthread -lpthread -lfltk 

clean:
	rm -f chat_server client_gui
