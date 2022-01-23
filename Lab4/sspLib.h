//Standard libraries
#include <iostream>
#include <vector>
#include <string>
#include <limits>       //when using string.find()
#include <regex.h>      //name check
#include <map>
#include <sstream>

//Networkbased
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>     //close()
#include <signal.h>      

//Const values and enums for server and client
const unsigned int MAXNAMELENGTH = 12;
const unsigned int MAXMSGSIZE = 512;
const std::string END = "<END>";

enum class RPSChoice
{ 
    NONE, 
    ROCK, 
    PAPER, 
    SCISSOR 
};

const std::map <RPSChoice, std::string> RPSChoiceText =
{
    {RPSChoice::NONE, "NONE"},
    {RPSChoice::ROCK, "ROCK"},
    {RPSChoice::PAPER, "PAPER"},
    {RPSChoice::SCISSOR, "SCISSOR"}
};

//Special keywords to send
enum class Keyword 
{
    OK,
    ERROR,
    QUIT,
    NONE
};

//The keyword will match what to send as a string
const std::map<Keyword, std::string> KEYS =
{
    {Keyword::OK,       "<OK>"},
    {Keyword::ERROR,    "<ERROR>"},
    {Keyword::QUIT,     "<QUIT>"}
};

enum class PlayerState
{
    NONE,
    MENU_SEND,
    MENU_RECV,
    GAMEQUEUE_SEND,
    GAMEQUEUE_RECV,
    GAMEQUEUE_READY,
    INGAME,
    HIGHSCORE_SEND,
    HIGHSCORE_RECV,
    QUIT
};

/*
    Push new data from a socket to the buffer
    If we got an empty message return false
    If we get an message with <QUIT> return false
*/
bool RecvMSG(const int& sock, std::string& buffer);

/*
    Get the msg that was first pushed into the buffer
*/
std::string GetMSG(std::string& buffer, bool removeMsg = false);

/*
    Delete the latest message from the buffer
*/
void EraseLastMSG(std::string& buffer);

/*
    Clear the buffer of all messages
*/
void ClearMSGQueue(std::string& buffer);

/*
    Send a message to a specific socket
    Prints out error if it failed...
*/
void SendMSG(const int& sock, std::string msg);

/*
    Returns NONE if no keyword could be found...
    Check the fulltext after the keywords
*/
Keyword GetKeyword(const std::string& fullText);

/*
    Returns the message in the fulltext
    If an error it will be printed and return will be empty
    Errors could be that keywords did not match
*/
std::string GetKeyMSG(const std::string& fullText, const Keyword& key);

/*
    Returns true if we got host and port.
    Gets host and port as parameters
*/
bool GetIpAndPort(struct sockaddr_storage addr, std::string& host, std::string& port);


/*
    Check if the username is acceptable
    Limited to 12 characters
    A-Z, a-z, 0-9 and _
    Returns an errorstring if there was something wrong
*/
std::string RegexNameCheck(const std::string& username);
