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
#include <arpa/inet.h>
#include <vector>

const unsigned int MAXNAMELENGTH = 12;
const unsigned int MAXMSGLENGTH = 255;
const std::string SERVERVERSION = "HELLO 1\n";
//Number of users that can be queued up at same time
const unsigned int BACKLOG = 5; 

class Server
{
private:
    bool m_isRunning;
    int m_sockfd;
    std::string m_serverIP;
    std::string m_serverPort;
    struct addrinfo* m_serverInfo;
    std::vector<int> m_clientSockets;
    std::vector<std::string> m_clientNames;

private:
    bool CheckName(std::string name);
    bool GetIpAndPort(struct sockaddr_storage addr, 
                      std::string& host, 
                      std::string& port);

public:
    Server();
    ~Server();

    bool Setup(char *argv[]);
    void Run();
};
