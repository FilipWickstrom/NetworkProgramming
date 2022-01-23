#include "client.h"

Client::Client()
{    
    m_isRunning = true;
    m_sockfd = 0;
    m_serverIP = "";
    m_serverPort = "";
    m_nickname = "";
}

Client::~Client()
{
    //Closing down the serversocket
    close(m_sockfd);
}

bool Client::Setup(char *argv[])
{
    /*-------Setup the nickname--------*/

    //Check username: Only 12 characters are okay
    if (strlen(argv[2]) > MAXNAMELENGTH)
    {
        std::cout << "Nickname was to long... Maxsize is: " << MAXNAMELENGTH << std::endl;
        return false;
    }
    m_nickname = argv[2];

    //Holds the acceptable characters that can be used
    regex_t regularexpression;

    //If regcomp returns 0 it was all good. Anything else means error...
    //A-Z, a-z, 0-9 and _ is okay characters
    if(regcomp(&regularexpression, "^[A-Za-z0-9_\\]+$", REG_EXTENDED))
    {
        std::cout << "Regex failed to compile..." << std::endl; 
        return false;
    }

    int error = regexec(&regularexpression, m_nickname.c_str(), size_t(0), NULL, 0);
    regfree(&regularexpression);

    //When error = 0 there is no errors
    if (error != 0)
    {
        std::cout << "Nickname: '" << m_nickname << "' is not accepted" << std::endl;
        return false;
    }

    /*-------Setup ip and port to the server--------*/

    //Gets ip:port as strings
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

    int status = getaddrinfo(m_serverIP.c_str(), m_serverPort.c_str(), &hints, &m_serverInfo);
    
    //Check if it was successful (0) and everything else is when failing
    if (status != 0)
    {
        //GAI = get address info
        std::cout << "GetAddrInfo failed... ERROR: " << gai_strerror(status) << std::endl;
        return false;
    }

    /*-------Connect to the server--------*/

    m_sockfd = socket(m_serverInfo->ai_family, 
                      m_serverInfo->ai_socktype, 
                      m_serverInfo->ai_protocol);
    
    if (m_sockfd < 0)
    {
        std::cout << "Failed to create socket..." << std::endl;
        return false;
    }

    int connfd = connect(m_sockfd, m_serverInfo->ai_addr, m_serverInfo->ai_addrlen); 
    if (connfd < 0)
    {
        std::cout << "Failed to connect to: " << argv[1] << std::endl;
        return false;
    }
    else
    {
        std::cout << "Connected to: " << argv[1] << std::endl;
    }

    /*-------Check information with server--------*/

    char serverVersion[16];
    int bytes = recv(m_sockfd, serverVersion, sizeof(serverVersion) - 1, 0);
    if (bytes < 0)
    {
        std::cout << "Did not get anything from server..." << std::endl;
        return false;
    }
    serverVersion[bytes] = '\0';
    std::cout << "Server protocol: " << serverVersion;

    //Check if client supports the servers version
    if (strcmp(serverVersion, CLIENTVERSION.c_str()) != 0)
    {
        std::cout << "Protocol did not work...Only supports: " << serverVersion << std::endl;
        return false;
    }

    //All good. Sending the nickname to server
    std::cout << "Protocol supported, sending nickname" << std::endl;

    //Sending nickname to server
    std::string name = "NICK " + m_nickname;
    if (send(m_sockfd, &name[0], name.length(), 0) < 0)
    {
        std::cout << "Nickname message to server failed..." << std::endl;
        return false;
    }

    //Check servers respons - OK Or ERR
    char serverNickResponsChar[32];
    bytes = recv(m_sockfd, &serverNickResponsChar[0], sizeof(serverNickResponsChar) - 1, 0);
    if (bytes < 0)
    {
        std::cout << "Nickname respons from server failed..." << std::endl;
        return false;
    } 
    serverNickResponsChar[bytes] = '\0';
    std::string serverNickRespons = serverNickResponsChar;

    if (strcmp(serverNickRespons.c_str(), "OK\n") == 0)
    {
        std::cout << "Nickname accepted" << std::endl;
    }
    else
    {
        std::cout << "Nickname not accepted... Server says: " << serverNickRespons << std::endl;
        return false;
    }

    return true;
}

void Client::Run()
{
    //Prepare select
    fd_set masterfds;
    fd_set readfds;
    FD_ZERO(&masterfds);
    FD_ZERO(&readfds);
    FD_SET(m_sockfd, &masterfds);
    FD_SET(fileno(stdin), &masterfds);

    while (m_isRunning)
    {       
        //Copy over the filedescriptors that will be used
        FD_ZERO(&readfds);
        readfds = masterfds;

        int selectReturn = select(m_sockfd + 1, &readfds, nullptr, nullptr, 0);
        if (selectReturn == -1)
        {
            std::cout << "ERROR... Select failed..." << std::endl;
        }
        else if (selectReturn == 0)
        {
            std::cout << "Got timed out... " << std::endl;  //Not relevant in this case
        }
        else
        {
            //Recieves and prints messages when it gets anything from the server
            if (FD_ISSET(m_sockfd, &readfds))
            {
                //Will recieve: MSG NICKNAME TheMessage\n
                int size = 4 + (MAXNAMELENGTH + 1) + MAXMSGLENGTH;
                char serverMSGChar[size];
                int bytes = recv(m_sockfd, serverMSGChar, sizeof(serverMSGChar) - 1, 0);
                if (bytes < 0)
                {
                    std::cout << "Failed to recieve message from server..." << std::endl;
                }
                else if (bytes == 0)
                {
                    std::cout << "Server shut down..." << std::endl;
                    m_isRunning = false;
                }
                serverMSGChar[bytes] = '\0';

                //Message from server follows this order: MSG Username message\n
                std::string serverMSG = serverMSGChar;

                //Go through all the MSG parts
                size_t msgIndex = -1;
                bool reachedEnd = false;
                std::vector<unsigned int> indexOfMSG;

                //Keep going until we reached end
                while (!reachedEnd)
                {
                    msgIndex = serverMSG.find("MSG", ++msgIndex);
                    if (msgIndex != std::numeric_limits<size_t>::max())
                    {
                        indexOfMSG.push_back(msgIndex);
                    }
                    else
                    {
                        reachedEnd = true;
                    }
                }

                //Split up the message in part
                for (size_t part = 0; part < indexOfMSG.size(); part++)
                {
                    std::string srvMSG = "";
                    int start = indexOfMSG[part];

                    //Last msg reads until end
                    if ((part + 1) == indexOfMSG.size())
                    {
                        srvMSG = serverMSG.substr(start);
                    }
                    else
                    {
                        int end = indexOfMSG[part + 1];
                        srvMSG = serverMSG.substr(start, end - start);
                    }

                    //Delete "MSG " part
                    srvMSG.erase(0, 4);
                    size_t index = srvMSG.find(" ");
                    if (index != std::numeric_limits<size_t>::max())
                    {
                        srvMSG.insert(index, ":");

                        //Final output on client: Username: message
                        std::cout << srvMSG;
                    }
                }
            }

            //Okay to write
            if (FD_ISSET(fileno(stdin), &readfds))
            {   
                std::string clientMSG;
                std::getline(std::cin, clientMSG);
                
                //Leave the server with some keywords
                if (clientMSG == "QUIT" || clientMSG == "LEAVE" || clientMSG == "KILL")
                {
                    std::cout << "Leaving server... Byebye!" << std::endl;
                    m_isRunning = false;
                }
                else
                {
                    //Check if message starts with "MSG" and add it if it's missing
                    size_t index = clientMSG.find("MSG");                    
                    if (index != 0)
                    {
                        clientMSG = "MSG " + clientMSG;
                    }

                    bool done = false;
                    std::vector<unsigned int> msgIndexes;
                    index = -1;

                    //Check how many MSG that will be send
                    while (!done)
                    {
                        index = clientMSG.find("MSG", ++index);
                        if (index != std::numeric_limits<size_t>::max())
                            msgIndexes.push_back(index);
                        else
                            done = true;
                    }

                    for (size_t msg = 0; msg < msgIndexes.size(); msg++)
                    {
                        std::string cliMSG = "";
                        int start = msgIndexes[msg];

                        //Last msg reads until end
                        if ((msg + 1) == msgIndexes.size())
                        {
                            cliMSG = clientMSG.substr(start);
                        }
                        else
                        {
                            int end = msgIndexes[msg + 1];
                            cliMSG = clientMSG.substr(start, end - start);
                        }

                        //If the message is longer than maximum, we cut of the end part
                        //+4 for "MSG "
                        if (cliMSG.length() > (MAXMSGLENGTH + 4))
                        {
                            cliMSG.erase(cliMSG.begin() + (MAXMSGLENGTH+4), cliMSG.end());
                            cliMSG.shrink_to_fit();
                        }  

                        // EXTRA that will not be used anyways...
                        /*
                        //Fix all the newlines in the message
                        size_t index = -1;
                        bool reachedEnd = false;
                        while (!reachedEnd)
                        {
                            index = cliMSG.find("\\n", ++index);
                            if (index != std::numeric_limits<size_t>::max())
                            {
                                //Replace '\' and 'n' with '\n'
                                cliMSG.erase(index, 2);
                                cliMSG.insert(index, 1, '\n');
                            }
                            else
                            {
                                reachedEnd = true;
                            }
                        }
                        */
                        
                        //Places a newline character at the end
                        cliMSG += '\n';

                        if (send(m_sockfd, &cliMSG[0], cliMSG.length(), 0) < 0)
                        {
                            std::cout << "Failed to send message to server..." << std::endl;
                        }
                    }
                }
            }
        }
    }

    //Clearing up
    FD_CLR(m_sockfd, &masterfds);
    FD_CLR(fileno(stdin), &masterfds);
    FD_CLR(m_sockfd, &readfds);
    FD_CLR(fileno(stdin), &readfds);
    FD_ZERO(&masterfds);
    FD_ZERO(&readfds);
}


int main(int argc, char *argv[])
{
    /*
    Input should be: 
    ./cchat ip:port Nickname
    Example: "./cchat 127.0.0.1:6660 Bob"
    */
    if (argc < 3 || argc > 3)
    {
        std::cout << "Arguments incorrect... Try: ./cchat ip:port Nickname" << std::endl;
        return 0;
    }

    Client client;
    
    //Connect to server and check the nickname
    if (client.Setup(argv))
    {
        //Ready to start chatting
        client.Run();
    }

    return 0;
}
