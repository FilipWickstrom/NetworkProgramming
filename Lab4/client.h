//Help functions
#include "sspLib.h"

const std::string PROTOCOL   = "RPS_TCP_2021";
volatile sig_atomic_t sigInt = 0;

class Client
{
private:
    bool m_isRunning;
    std::string m_username;
    int m_serverSock;
    std::string m_msgBuffer;

//Helpfunctions
private:
    int PrepareSet(fd_set& fds);
    void CloseSet(fd_set& fds);
    static void SignumCall(int signum);

public:
    Client();
    ~Client();

    bool Setup(std::string host, std::string port, std::string username);
    void Run();
};
