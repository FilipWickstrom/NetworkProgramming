#include "client.h"

Client::Client()
{
    m_isRunning = true;
    m_username = "";
    m_serverSock = 0;
    m_msgBuffer = "";
}

Client::~Client()
{
    close(m_serverSock);
}

int Client::PrepareSet(fd_set& fds)
{
    FD_ZERO(&fds);
    FD_SET(m_serverSock, &fds);
    FD_SET(STDIN_FILENO, &fds);
    
    int maxFd = m_serverSock;
    if (STDIN_FILENO > m_serverSock)
        maxFd = STDIN_FILENO;

    return maxFd;
}

void Client::CloseSet(fd_set& fds)
{
    FD_CLR(m_serverSock, &fds);
    FD_CLR(STDIN_FILENO,&fds);
    FD_ZERO(&fds);
}

void Client::SignumCall(int signum)
{
    sigInt = signum;   
}

bool Client::Setup(std::string host, std::string port, std::string username)
{
    std::string serverMSG = "";
    std::string errorMsg = RegexNameCheck(username);
    if (errorMsg != "")
    {
        std::cout << errorMsg << std::endl;
        return false;
    }
    m_username = username;

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;        //IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    //TCP protocol

    struct addrinfo* results = nullptr;
    struct addrinfo* current = nullptr;

    int error = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
    if (error != 0)
    {
        std::cout << "GetAddrInfo failed... ERROR Code: " << gai_strerror(error) << std::endl;
        return false;
    }

    struct sockaddr_storage* serverAddr = nullptr;

    //Got through all the address that can fit
    for (current = results; current != nullptr; current->ai_next)
    {
        m_serverSock = socket(current->ai_family, 
                              current->ai_socktype, 
                              current->ai_protocol);

        if (m_serverSock < 0)
            continue;

        //Successfully binded the socket if it was lower than 0
        if (connect(m_serverSock, current->ai_addr, current->ai_addrlen) == 0)
        {
            serverAddr = (sockaddr_storage*)current->ai_addr;
            break;
        }
        close(m_serverSock);
    }
    freeaddrinfo(results);

    std::string hostname = "";
    std::string portname = "";
    if (GetIpAndPort(*serverAddr, hostname, portname))
    {
        std::cout << "[+] Connected to server on address: " << hostname << " and port: " << portname << std::endl; 
    }
    else
    {
        std::cout << "[-] Could not get ip and port..." << std::endl;
        return false;
    }

    //Send this protocol - server checks if both are the same
    //If it was same, server sends OK otherwise ERROR...
    SendMSG(m_serverSock, (std::string&)PROTOCOL); 

    if (!RecvMSG(m_serverSock, m_msgBuffer))
    {   
        std::cout << "Server closed down..." << std::endl;
        return false;
    }
    serverMSG = GetMSG(m_msgBuffer, true); 
    if (serverMSG == "")
        return false;
    Keyword serverKey = GetKeyword(serverMSG);

    //Protocol was OK
    if (serverKey == Keyword::OK)
    {
        std::cout << "[+] Protocol: OK" << std::endl;

        //Send username to server
        SendMSG(m_serverSock, m_username);

        if (!RecvMSG(m_serverSock, m_msgBuffer))
        {
            std::cout << "Server closed down..."  << std::endl;
            return false;
        }
        serverMSG = GetMSG(m_msgBuffer, true);
        if (serverMSG == "")
            return false;
        serverKey = GetKeyword(serverMSG);
        if (serverKey == Keyword::OK)
        {
            std::cout << "[+] Nickname: OK" << std::endl;
            std::cout << "[+] Joined the server\n" << std::endl;
            /*
                Finally ready to go!!! :D
            */
        }
        else if (serverKey == Keyword::ERROR)
        {
            std::cout << "[-] Nickname: '" << GetKeyMSG(serverMSG, Keyword::ERROR) << "'" << std::endl;
            return false;
        }
    }
    //Protocol was not okay... Some error occured
    else if (serverKey == Keyword::ERROR)
    {
        std::cout << "[-] Protocol: '" << GetKeyMSG(serverMSG, Keyword::ERROR) << "'" << std::endl;
        return false;
    }
    return true;
}

void Client::Run()
{
    fd_set readfds;
    int maxFd = 0;
    std::string serverMSG = "";
    
    //Let us know when we press Ctrl+C
    signal(SIGINT, SignumCall);
    
    while(m_isRunning)
    {
        //Print out the latest from the buffer if anything exists
        serverMSG = GetMSG(m_msgBuffer, true);
        if (serverMSG != "")
        {
            switch (GetKeyword(serverMSG))
            {
            case Keyword::QUIT:
                std::cout << "Leaving the server..." << std::endl;
                m_isRunning = false;
                break;
            case Keyword::ERROR:
                std::cout << "ERROR: " << GetKeyMSG(serverMSG, Keyword::ERROR) << std::endl;
                break;
            default:
                std::cout << serverMSG << std::endl;
                break;
            };
        }

        //Avoid doing stuff if it already has been turned off
        if (m_isRunning)
        {
            //Preparing the set of file descriptors
            maxFd = PrepareSet(readfds);
            //10ms wait time
            struct timeval tv = {0, 10000};

            switch (select(maxFd + 1, &readfds, nullptr, nullptr, &tv))
            {
            case -1:
                //If sigibt was detected we shall close down
                if (sigInt == SIGINT)
                {
                    std::cout << "\nSending leavemessage to server" << std::endl;
                    SendMSG(m_serverSock, KEYS.at(Keyword::QUIT));
                }
                m_isRunning = false;
                break;
            case 0:
                break;
            default:
                //If the server sends a message we sends it to the clients buffer
                if (FD_ISSET(m_serverSock, &readfds))
                {
                    if (!RecvMSG(m_serverSock, m_msgBuffer))
                    {
                        if (m_msgBuffer.empty())
                        {
                            std::cout << "Server closed down..." << std::endl;
                            m_isRunning = false;
                        }
                    }
                }

                //Send - Can send whatever they want to server
                if (FD_ISSET(STDIN_FILENO, &readfds))
                {
                    std::string answer = "";
                    std::getline(std::cin, answer);
                    SendMSG(m_serverSock, answer);
                }
                break;
            }
        }
    }

    //Cleaning up the set of socket and stdin
    CloseSet(readfds);
}

/*
Start with: executableFile Address Port Username
Example:
./sspgame 127.0.0.1 6660 Spiderman
./sspgame localhost 6660 Batman
./sspgame ::1 6660 Ironman
*/
int main(int argc, char *argv[])
{
    //Check that input was correct
    if (argc < 4 || argc > 4)
    {
        std::cout << "Wrong input... Expected: ./sspd Address Port Username" << std::endl;
        return 0;
    }

    Client client;
    if (client.Setup(argv[1], argv[2], argv[3]))
        client.Run();
    else
        std::cout << "Client setup failed... " << std::endl;

    return 0;
}
