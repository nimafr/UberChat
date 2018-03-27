// This example will demonstrate the integration of
// the boost asio chatserver with a simple FLTK program
//
//
//
#include <assert.h>

#include "client_cli.cpp"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Display.H>
#include <string>
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Bar.H>

Fl_Window win   (600, 500, "UberChat");
Fl_Input input1 (210, 450, 200, 20, "In: ");
Fl_Button quit  (410, 450, 50,20,"Quit");
Fl_Button clear (470, 450, 50,20,"Clear");

Fl_Text_Buffer *buff = new Fl_Text_Buffer ();
Fl_Text_Display *disp = new Fl_Text_Display (190,50,400,400,"chat");

Fl_Text_Buffer *buff2 = new Fl_Text_Buffer ();
Fl_Text_Display *disp2 = new Fl_Text_Display (200,0,200,30,"Current Chatroom");

Fl_Text_Buffer *buff1 = new Fl_Text_Buffer ();
Fl_Text_Display *disp1 = new Fl_Text_Display (2,50,180,250,"Chatroom members");

//Fl_Return_Button *b_nick = new Fl_Return_Button(10, 330, 170, 25, "NICK");
Fl_Return_Button *b_change_room = new Fl_Return_Button(10, 365, 170, 25, "CHANGECHATROOM");
Fl_Return_Button *b_req_room = new Fl_Return_Button(10, 400, 170, 25, "REQCHATROOMS");
Fl_Return_Button *b_name_room = new Fl_Return_Button(10, 435, 170, 25, "NAMECHATROOM");

//creation of a MENUBAR
Fl_Menu_Bar *menubar;

void QuitCB(Fl_Widget* w, void* p) {win.hide();}
void CommandListCB(Fl_Widget* w, void* p) {
  char const *help = "Uber Chat Help Menu\n\n"
                      "How to Use:\n\n"
                      "In order to use, the user has to click on a button at the bottom left corner of the main\n"
                      "window. once the button is pressed, the user has to type in a response in the input box\n"
                      "labeled as \"in\". If a button is not pressed, the \"in\" box will treat the input as \n"
                      "a message to be sent.\n\n"
                      "Command Definitions:\n\n"
                      "NICK : Lets you create a nickname to be seen by the other participants in the same chatroom\n"
                      "CHANGECHATROOM : Lets the user enter a different existing chatroom in the same server\n"
                      "REQCHATROOMS : Gives you a list of all the currently available chatrooms in the same server\n"
                      "NAMECHATROOM : Lets the you create a new chatroom with a name give by the user";
  fl_message(help);
}

Fl_Menu_Item menuitems[] = {
	{ "&Window", 0, 0, 0, FL_SUBMENU},

	  { "&Quit", FL_ALT + 'q', (Fl_Callback *) QuitCB},
	  { 0 },
  { "&Help", 0, 0, 0, FL_SUBMENU},
	  { "&Command List", 0, (Fl_Callback *) CommandListCB},
    { 0 },
	{ 0 }
};
//end of creation of MENUBAR


void clearmembers ();
static void cb_clear ();
static void room_clear();
void printrequest (std::string payload);
void dispRoom();
int function = 0;
// boost asio instances
chat_client *c = NULL;
std::thread *t = NULL;
std::vector< std::vector<std::string> >uuidnames; // hold uuid and names

  void storename(std::string name, std::string id)
  {
    std::cout << name <<"*09**"<< std::endl;
    std::cout << id <<"*87**"<< std::endl;
    std::vector<std::string>uuid;
    uuid.push_back(id);
    uuid.push_back(name);
    uuidnames.push_back(uuid);

  } //uuidnames[i].size()

  std::string getname(std::string id)
  {
    std::string ans;
    bool found= true;
    for (int i=0; i < uuidnames.size() &&  found; i++)
    {
      for (int j=0; j < uuidnames.size()&&  found; j++)
      {
        if(uuidnames[i][0]==id)
        {
          ans = uuidnames[i][1];
          found = false;
        }
      }
    }
    return ans;
  }

  void setnameid(std::string info)
  {
    int comma = info.find(',');
    while (comma != -1)
    {
      std::string uui = info.substr(0,comma);
      info.erase(0,comma+1);

      comma =0;
      comma=info.find('_');
      std::string username = info.substr(0,comma);
      storename(username, uui);
      username =username + "\n";

      info.erase(0,comma+2);
      if (buff1)
      {
        buff1->append ( username.c_str () );
      }
      if (disp1)
      {
        disp1->show ();
      }
      win.show ();
      comma =0;
      comma=info.find(',');
    }
  }
    void removector()
    {
        int Siz = uuidnames.size();
         uuidnames.erase(uuidnames.begin(),uuidnames.begin()+Siz );

    }


static void cb_recv ( chat_message cm )
{
  // Note, this is an async callback from the perspective
  // of Fltk..
  //
  // high chance of a lock needed here if certain fltk calls
  // are made.  (like show() .... )

  std::string S;

  boost::crc_32_type checksum;
  boost::posix_time::ptime time;
  std::string payload;
  std::string command;

  bool cksum_ok = parse_command ( cm, &checksum, &time, &command, &payload );

   if (command == "REQUUID"||command == "NICK"||command == "NAMECHATROOM")
    {
      chat_message  msg = format_reply ( "GETNICK");
      payload = payload + " \n";

      if (buff)
      {
        buff->append ( payload.c_str () );
      }
      if (disp)
      {
        disp->show ();
        disp->Fl_Text_Display::move_down();
      }
        win.show ();
      }
      else if (command == "REQUSERS"){
        setnameid(payload );

      }

    else if (command == "REQTEXT")
    {
      std::string content = payload + "\n";
      int ANS = content.find('-');
      while (ANS != -1)
      {
        ANS = content.find(' ');
        std::string uuid = content.substr(0,ANS);
        std::string nick= getname(uuid);
        content.erase(0,ANS+1);
        ANS = 0;
        ANS = content.find(';');
        std::string messageout=content.substr(0,ANS);
        content.erase(0,ANS+1);
        messageout = "["+nick+"]: " + messageout + " \n";


        if (buff)
        {
            buff->append ( messageout.c_str () );
        }
        if (disp)
        {
            disp->show ();
            disp->Fl_Text_Display::move_down();
        }

        win.show ();
        ANS = 0;
        ANS = content.find('-');
      }
    }
    else if (command == "REQCHATROOM"){
      if (buff2)
      {
        buff2->append (payload.c_str ());
      }
      if (disp2)
      {
        disp2->show ();
      }
      win.show ();
    }


  else if (command == "CHANGECHATROOM")
  {
    clearmembers ();
    removector();
    cb_clear ();
    room_clear();

    payload = payload + " \n";
    printrequest(payload);

    std::string comm = "REQCHATROOM";
    chat_message wins;
    wins = format_reply(comm);
    wins.encode_header();
    c->write(wins);

  }

 else if (command == "REQCHATROOMS")
  {
     payload = payload + " \n";
    int a = payload.find(';');
    printrequest(payload);
  }


}

 void printrequest (std::string payload) {       //prints to chat main window!!!!
   if (buff)
      {
        buff->append ( payload.c_str () );
      }
      if (disp)
      {
        disp->show ();
        disp->Fl_Text_Display::move_down();
      }
        win.show ();
     }

  void clearmembers ()
  {
    if (buff1)
    {
      buff1->remove (0,  buff1->length () );
      removector();
    }
    // may need to call show() ?

  }

  static void cb_clear ()
  {
    if (buff)
    {
      buff->remove (0,  buff->length () );
    }
    // may need to call show() ?

  }

  static void room_clear()
  {
    if (buff2)
    {
      buff2->remove (0, buff2->length());
    }
  }

static void cb_quit ( )
{
  // this is where we exit to the operating system
  // any clean up needs to happen here
  //
  if (c)
  {
    c->close();
  }
  if (t)
  {
     t->join();
  }
  exit (0);
}
static void cb_input1 (Fl_Input*, void * userdata)
{
  chat_message msg;
  msg.body_length(std::strlen( ( const char *) input1.value ()) + 1);

  std::string temp = input1.value();

  switch(function){
    case 0:
      msg = format_reply ( "SENDTEXT, " + temp);
    break;

    case 1:
      msg = format_reply ( "NICK, " + temp);
    break;

    case 2:
      msg = format_reply ( "CHANGECHATROOM, " + temp);
    break;

    case 4:
      msg = format_reply ( "NAMECHATROOM, " + temp);
    break;
  }
  msg.encode_header();
  std::cout << "sent " << msg.body() << std::endl;
  c->write(msg);
  function = 0;
  sleep (1);
}

void update(void*){
  chat_message rqtxt;
  rqtxt = format_reply("REQTEXT");
  rqtxt.encode_header();
  c->write(rqtxt);


  Fl::repeat_timeout(0.5, update);
}

void updateUsers(void*){
  clearmembers();
  chat_message rqusers;
  rqusers = format_reply("REQUSERS");
  rqusers.encode_header();
  c->write(rqusers);

  Fl::repeat_timeout(5, updateUsers);
}

void rqUUID(void*){

  chat_message command;
  command = format_reply("REQUUID");
  command.encode_header();
  c->write(command);
}
void nICK(void*){
  std::string nickname;
  int found;
  do{
  nickname = fl_input("Welcome!\n Please type in a Nickname:", "NICK HERE");
  found = nickname.find_first_of(".,/\\-=][-_~");
  if(found != -1)
  {fl_message("Nicknames with delimiter characters are not accepted\n"
              "Please, type in a different nickname");}
  }while(found != -1);
  std::string commANDnick = "NICK," + nickname;
  chat_message nICK;
  nICK = format_reply(commANDnick);
  nICK.encode_header();
  c->write(nICK);
}
void showRoom(void*){
  std::string comm = "REQCHATROOM";
  chat_message win;
  win = format_reply(comm);
  win.encode_header();
  c->write(win);
}

void function_1(Fl_Widget* w, void* p){
  function = 1;
}

void function_2(Fl_Widget* w, void* p){
  function = 2;
}

void function_3(Fl_Widget* w, void* p){
    chat_message msg;
  msg = format_reply ( "REQCHATROOMS");
  msg.encode_header();
  c->write(msg);
  sleep(1);

  function = 0;
}

void function_4(Fl_Widget* w, void* p){
  function = 4;
}

void function_5(Fl_Widget* w, void* p){
  function = 5;
}

int main ( int argc, char **argv)
{
  win.begin ();
  win.add (input1);

  Fl::add_timeout(0.5, rqUUID);
  Fl::add_timeout(1, nICK);
  Fl::add_timeout(1, updateUsers);
  Fl::add_timeout(2, update);
  Fl::add_timeout(1, showRoom);
  input1.callback ((Fl_Callback*)cb_input1,( void *) "Enter next:");
  input1.when ( FL_WHEN_ENTER_KEY );
  b_change_room -> callback((Fl_Callback *) function_2, 0);
  b_req_room -> callback((Fl_Callback *) function_3, 0);
  b_name_room -> callback((Fl_Callback *) function_4, 0);
  quit.callback (( Fl_Callback*) cb_quit );
  clear.callback (( Fl_Callback*) cb_clear );
  win.add (quit);
  disp->buffer(buff);
  disp1->buffer(buff1);
  disp2->buffer(buff2);
  //MENUBAR
  const int x = 160;
  menubar = new Fl_Menu_Bar(0, 0, x, 25);
  menubar->menu(menuitems);
  //
  //enables resizing of window
  win.resizable(win);
  //end of window resizing
  win.end ();
  win.show ();

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
    c = new chat_client (io_service, endpoint_iterator, &cb_recv);

    t = new std::thread ([&io_service](){ io_service.run(); });

    // goes here, never to return.....
    return Fl::run ();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  // never gets here
  return 0;
}
