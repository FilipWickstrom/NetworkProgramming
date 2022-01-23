#include <stdio.h>
#include <stdlib.h>
#include <iostream>     //In- and Output
#include <string.h>     
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>     //close()
#include <regex.h>      //Use specific characters
#include <limits>
#include <vector>

const unsigned int MAXNAMELENGTH = 12;
const unsigned int MAXMSGLENGTH = 255;
const std::string CLIENTVERSION = "HELLO 1\n";

class Client
{
private:
    bool m_isRunning;
    std::string m_nickname;
    int m_sockfd;
    std::string m_serverIP;
    std::string m_serverPort;
    struct addrinfo* m_serverInfo;

public:
    Client();
    ~Client();

    bool Setup(char *argv[]);
    void Run();
};
