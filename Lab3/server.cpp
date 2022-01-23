#include "server.h"

Server::Server()
{
    m_isRunning = true;
    m_serverInfo = nullptr;
}

Server::~Server()
{
    for (size_t i = 0; i < m_clientSockets.size(); i++)
    {
        close(m_clientSockets[i]);
    }
    m_clientSockets.clear();
    m_clientNames.clear();
}

bool Server::CheckName(std::string name)
{
    if (strlen(name.c_str()) > MAXNAMELENGTH)
    {
        std::cout << "Nickname was to long..." << std::endl;
        return false;
    }

    //Holds the acceptable characters that can be used
    regex_t regularexpression;

    //If regcomp returns 0 it was all good. Anything else means error...
    //A-Z, a-z, 0-9 and _ is okay characters
    if(regcomp(&regularexpression, "^[A-Za-z0-9_\\]+$", REG_EXTENDED))
    {
        std::cout << "Regex failed to compile..." << std::endl; 
        return false;
    }

    int error = regexec(&regularexpression, name.c_str(), size_t(0), NULL, 0);
    regfree(&regularexpression);

    //When error = 0 there is no errors
    if (error != 0)
    {
        std::cout << "Nickname: '" << name << "' is not accepted" << std::endl;
        return false;
    }

    return true;
}

bool Server::GetIpAndPort(struct sockaddr_storage addr, 
                          std::string& host, 
                          std::string& port)
{
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];
    socklen_t addr_size = sizeof(addr);

    int rc = getnameinfo((struct sockaddr *)&addr, 
                        addr_size, hoststr, sizeof(hoststr), 
                        portstr, sizeof(portstr), 
                        NI_NUMERICHOST | NI_NUMERICSERV);

    bool status = false;
    if (rc == 0)
    {
        host = hoststr;
        port = portstr;
        status = true;
    }
    return status;
}

bool Server::Setup(char *argv[])
{
    std::string ipAndPort = argv[1];
    size_t colonIndex = ipAndPort.find(":");
    if (colonIndex == std::numeric_limits<size_t>::max())
    {
        return false;
    } 
    m_serverIP = ipAndPort.substr(0,colonIndex);
    m_serverPort = ipAndPort.substr(colonIndex+1, ipAndPort.size());

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;        //IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    //Mostly used for TCP
    hints.ai_protocol = IPPROTO_TCP;    //TCP protocol

    addrinfo* res;
    //Gets a whole list with what is possible
    int status = getaddrinfo(m_serverIP.c_str(), m_serverPort.c_str(), &hints, &res);
    if (status != 0)
    {
        //GAI = get address info
        std::cout << "GetAddrInfo failed... ERROR: " << gai_strerror(status) << std::endl;
        return false;
    }
    
    //Looping until successfull bind
    for (m_serverInfo = res; m_serverInfo != nullptr; m_serverInfo->ai_next)
    {
        m_sockfd = socket(m_serverInfo->ai_family, 
                          m_serverInfo->ai_socktype, 
                          m_serverInfo->ai_protocol);

        if (m_sockfd < 0)
            continue;

        //Found the correct address
        if (bind(m_sockfd, m_serverInfo->ai_addr, m_serverInfo->ai_addrlen) == 0)
        {
            break;
        }
        close(m_sockfd);
    }
    //Freeing the list
    freeaddrinfo(res);

    //Use same address and port for every connection
    int yes = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes)))
    {
        std::cout << "Failed to setup socket options..." << std::endl;
        return false;
    }

    if (listen(m_sockfd, BACKLOG) < 0)
    {
        std::cout << "Failed to setup listen..." << std::endl;
        return false;
    }
    std::cout << "Listening on address: " << m_serverIP << " and port: " << m_serverPort << std::endl;
    
    return true;
}

void Server::Run()
{
    fd_set readfds;
    int bytes = 0;

    //Keep going until we turn it off
    while (m_isRunning)
    {
        FD_ZERO(&readfds);
        FD_SET(m_sockfd, &readfds);
        int maxFd = m_sockfd;

        //Get max filedescriptor and set to readset
        for (size_t i = 0; i < m_clientSockets.size(); i++)
        {
            FD_SET(m_clientSockets[i], &readfds);
            
            if (m_clientSockets[i] > maxFd)
                maxFd = m_clientSockets[i];
        }
        
        int selectReturn = select(maxFd + 1, &readfds, nullptr, nullptr, 0);

        if (selectReturn == -1)
        {
            std::cout << "ERROR... Select failed..." << std::endl;
        }
        else if (selectReturn == 0)
        {
            std::cout << "Got timed out... " << std::endl;
        }
        else
        {
            //Main socket that listen to new users
            if (FD_ISSET(m_sockfd, &readfds))
            {
                struct sockaddr_storage client_addr;
                socklen_t client_size = sizeof(client_addr);

                int newsock = accept(m_sockfd, (struct sockaddr *)&client_addr, &client_size);
                if (newsock >= 0)
                {
                    std::string host, port;
                    if (GetIpAndPort(client_addr, host, port))
                        std::cout << "Client connected from address: " << host << " port: " << port << std::endl;
                    else
                        std::cout << "Failed to get address and port..." << std::endl;

                    //Sending protocol that server uses
                    if (send(newsock, &SERVERVERSION[0], SERVERVERSION.length(), 0) >= 0)
                    {
                        //Recieve a nickname - NICK <nickname>
                        char nicknameChar[100];
                        bytes = recv(newsock, nicknameChar, sizeof(nicknameChar) - 1, 0);

                        if (bytes >= 0)
                        {
                            nicknameChar[bytes] = '\0';
                            std::string nickname = nicknameChar;

                            size_t index = nickname.find("NICK");
                            if (index != std::numeric_limits<size_t>::max())
                            {
                                //Strip down "NICK" in front
                                index = nickname.find(" ");
                                if (index != std::numeric_limits<size_t>::max())
                                {
                                    nickname.erase(0, index + 1);
                                }
                                index = nickname.find("\n");
                                if (index != std::numeric_limits<size_t>::max())
                                {
                                    nickname.erase(nickname.begin() + index, nickname.end());
                                }

                                std::string nameFeedback;
                                if (CheckName(nickname))
                                {
                                    std::cout << "Nickname '" << nickname << "' accepted" << std::endl;
                                    nameFeedback = "OK\n";

                                    //Add to the vector of sockets
                                    m_clientSockets.push_back(newsock);
                                    m_clientNames.push_back(nickname);
                                    std::cout << "Users in chat: " << m_clientSockets.size() << std::endl;
                                }
                                else
                                {
                                    std::cout << "Nickname '" << nickname << "' NOT accepted..." << std::endl;
                                    nameFeedback = "ERROR <Nickname not okay...>\n";
                                }

                                if (send(newsock, &nameFeedback[0], nameFeedback.length(), 0) < 0)
                                {
                                    std::cout << "Failed to send feedback to client..." << std::endl;
                                }
                            }
                            else
                            {
                                std::cout << "ERROR <NICK was not sent...>" << std::endl;
                            }
                        }
                        else 
                        {
                            std::cout << "Failed to recv nickname from client..." << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "Failed to send protocol to client..." << std::endl;
                    }
                }
                else
                {
                    std::cout << "Failed to accept..." << std::endl;
                    close(newsock);
                }
            }

            //Go through all the sockets and see if anyone wrote anything
            for (size_t i = 0; i < m_clientSockets.size(); i++)
            {
                int currentSock = m_clientSockets[i];

                //Check if any socket has said something
                if (FD_ISSET(currentSock, &readfds))
                {
                    //Final message: MSG NICKNAME THEMESSAGE
                    int msgSize = 4 + (MAXNAMELENGTH + 1) + MAXMSGLENGTH;
                    char clientMSGChar[msgSize];

                    bytes = recv(currentSock, clientMSGChar, sizeof(clientMSGChar) - 1, 0);
                    //Recv fail
                    if (bytes < 0)
                    {
                        std::cout << "Failed to get message from client..." << std::endl;
                    }
                    //Left the chat
                    else if (bytes == 0)
                    {
                        std::cout << m_clientNames[i] << " left the chat..." << std::endl;
                        close(m_clientSockets[i]);
                        m_clientSockets.erase(m_clientSockets.begin() + i);
                        m_clientNames.erase(m_clientNames.begin() + i);
                        std::cout << "Users in chat: " << m_clientSockets.size() << std::endl;
                    }
                    //Got the message
                    else
                    {
                        clientMSGChar[bytes] = '\0';
                        std::string clientMessage = clientMSGChar;
                        size_t msgIndex = -1;
                        bool reachedEnd = false;
                        std::vector<unsigned int> indexOfMSG;

                        //Keep going until we reached end
                        while (!reachedEnd)
                        {
                            msgIndex = clientMessage.find("MSG", ++msgIndex); 
                            if (msgIndex != std::numeric_limits<size_t>::max())
                            {
                                indexOfMSG.push_back(msgIndex);
                            }
                            else
                            {
                                reachedEnd = true;
                            }
                        }

                        //Split up the total message to different parts
                        for (size_t part = 0; part < indexOfMSG.size(); part++)
                        {
                            std::string cliMSG = "";
                            int start = indexOfMSG[part];            
                
                            //Last msg reads until end
                            if ((part + 1) == indexOfMSG.size())
                            {
                                cliMSG = clientMessage.substr(start);
                            }
                            else
                            {
                                int end = indexOfMSG[part+1];
                                cliMSG = clientMessage.substr(start, end-start);
                            }

                            //Send the message
                            size_t index = cliMSG.find(" ");
                            if (index != std::numeric_limits<size_t>::max())
                            {
                                cliMSG.insert(index, " " + m_clientNames[i]);

                                //Send message to everyone else - except for the sender of the message
                                for (size_t sock = 0; sock < m_clientSockets.size(); sock++)
                                {
                                    if (currentSock != m_clientSockets[sock])
                                    {
                                        if (send(m_clientSockets[sock], &cliMSG[0], cliMSG.length(), 0) < 0)
                                        {
                                            std::cout << "Failed to send message..." << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                        //None of the keywords: "MSG" was found... Send back error...
                        if (indexOfMSG.empty())
                        {
                            std::string errorMsg = "ERROR malformed command\n";
                            if (send(currentSock, &errorMsg[0], errorMsg.length(), 0) < 0)
                            {
                                std::cout << "Failed to send message..." << std::endl;
                            }
                        }
                    }
                }
            }
        }
        
        //Clearing
        FD_CLR(m_sockfd, &readfds);
        for (size_t i = 0; i < m_clientSockets.size(); i++)
        {
            FD_CLR(m_clientSockets[i], &readfds);
        } 
        FD_ZERO(&readfds);
    }
}

int main(int argc, char *argv[])
{
    /*
    Input should be: 
    ./cserverd ip:port
    Example: "./cserverd 127.0.0.1:6660"
    */
    if (argc < 2 || argc > 2)
    {
        std::cout << "Arguments incorrect... Try: ./cserverd ip:port" << std::endl;
        return 0;
    }

    Server server;
    
    //Starting up the server
    if (server.Setup(argv))
    {
        //Ready to take clients
        server.Run();
    }
    std::cout << "Closing down server..." << std::endl;
    return 0;
}
