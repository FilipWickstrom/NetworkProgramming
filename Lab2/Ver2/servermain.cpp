#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

// My includes
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <vector>       //Holding current clients

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

/*
UDP server that sends binary with multiple clients
Programmed by Filip Wickström
Last update: 2021-02-22
*/

//10 * 3 = 30 seconds max wait
#define MAXWAIT 10
#define LOOPSWITHOUTRECV 3

#define MSGMAXSIZE 50
#define TRUE 1
#define FALSE 0

void SetupServersocket(char *input, int &sockfd)
{
    //Hints, is what to follow
    struct addrinfo hints;
    struct addrinfo* res = nullptr;
    struct addrinfo* addr = nullptr;
    memset(&hints, 0, sizeof(hints));

    //IPv4 or IPv6 and UDP
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    //Input format: <IP>:<PORT>
    const char delim[2] = ":";
    char *ip = strtok(input, delim);  //Reads until ':'
    char *port = strtok(NULL, delim); //Reads the rest
    
    //Gets a whole list with what is possible
    if (getaddrinfo(ip, port, &hints, &res) != 0)
    {
        printf("Getaddrinfo failed...\n");
        exit(1);
    }
    
    //Looping until successfull bind
    for (addr = res; addr != nullptr; addr->ai_next)
    {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

        if (sockfd == -1)
            continue;

        if (bind(sockfd, addr->ai_addr, addr->ai_addrlen) == 0)
        {
            printf("+Server socket created\n");
            printf("+Server socket binded\n");
            break;
        }

        close(sockfd);
    }
    //Freeing the list
    freeaddrinfo(res);

    //Failed to bind...
    if (addr == nullptr)
    {
        printf("Could not find something to bind to...\n");
        exit(1);
    }

    //Set sockoptions - Should only listen on recieve for maximum 10 seconds
    struct timeval timer;
    timer.tv_sec = MAXWAIT;
    timer.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&timer, sizeof(timeval)) == -1)
    {
        printf("Failed to setup 'SO_RCVTIMEO'...\n");
        close(sockfd);
        exit(1);
    }
    else
    {
        printf("+Server 'SO_RCVTIMEO' set\n");
    }

    printf("-----Setup done!-----\n\n");
}

//Conversions
void ConvertNetToHost(calcMessage &msg)
{
    //Short - ntohs
    msg.type = ntohs(msg.type);
    msg.major_version = ntohs(msg.major_version);
    msg.minor_version = ntohs(msg.minor_version);
    msg.protocol = ntohs(msg.protocol);

    //Long - ntohl
    msg.message = ntohl(msg.message);
}

void ConvertNetToHost(calcProtocol &proto)
{
    //Short - ntohs conversion
    proto.type = ntohs(proto.type);
    proto.major_version = ntohs(proto.major_version);
    proto.minor_version = htons(proto.minor_version);

    //Long - ntohl conversion
    proto.arith = ntohl(proto.arith);
    proto.id = ntohl(proto.id);
    proto.inValue1 = ntohl(proto.inValue1);
    proto.inValue2 = ntohl(proto.inValue2);
    proto.inResult = ntohl(proto.inResult);

    //Do nothing with the "floats"****
}

void ConvertHostToNet(calcProtocol &proto)
{
    //Short - htons conversion
    proto.type = htons(proto.type);
    proto.major_version = htons(proto.major_version);
    proto.minor_version = htons(proto.minor_version);

    //Long - htonl conversion
    proto.arith = htonl(proto.arith);
    proto.id = htonl(proto.id);
    proto.inValue1 = htonl(proto.inValue1);
    proto.inValue2 = htonl(proto.inValue2);
    proto.inResult = htonl(proto.inResult);

    //Do nothing with the "floats"
}

//Just to make it a bit cleaner
void SendCalcMsg(int sockfd, sockaddr clientaddr, calcMessage msg, int id)
{
    int sendBytes = sendto(sockfd, &msg, sizeof(msg), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
    if (sendBytes < 0)
    {
        printf("Server -> Client[%d]: Failed to send a message...\n", id);
    }
    else
    {
        printf("Server -> Client[%d]: Sent a message\n", id);
    }
}

void SendCalcProto(int sockfd, sockaddr clientaddr, calcProtocol proto, int id)
{
    int sendBytes = sendto(sockfd, &proto, sizeof(proto), 0, (const struct sockaddr *)&clientaddr, sizeof(clientaddr));
    if (sendBytes < 0)
    {
        printf("Server -> Client[%d]: Failed to send a protocol\n", id);
    }
    else
    {
        printf("Server -> Client[%d]: Sent a protocol\n", id);
    }
}

calcProtocol GetNewProtocol(int id)
{
    calcProtocol proto;
    memset(&proto, 0, sizeof(calcProtocol)); //Set everything to 0

    //Initialize the library
    initCalcLib();
    char *arith = randomType();

    //If float
    if (arith[0] == 'f')
    {
        double f1 = randomFloat();
        double f2 = randomFloat();
        double result = 0.0f;

        if (strcmp(arith, "fadd") == 0)
        {
            result = f1 + f2;
            proto.arith = 5;
        }
        else if (strcmp(arith, "fsub") == 0)
        {
            result = f1 - f2;
            proto.arith = 6;
        }
        else if (strcmp(arith, "fmul") == 0)
        {
            result = f1 * f2;
            proto.arith = 7;
        }
        else if (strcmp(arith, "fdiv") == 0)
        {
            result = f1 / f2;
            proto.arith = 8;
        }

        proto.flValue1 = f1;
        proto.flValue2 = f2;
        proto.flResult = result;
        printf("Server -> Client[%d]: %f %s %f = %f\n", id, f1, arith, f2, result);
    }
    //Else in
    else
    {
        int i1 = randomInt();
        int i2 = randomInt();
        int result = 0;

        if (strcmp(arith, "add") == 0)
        {
            result = i1 + i2;
            proto.arith = 1;
        }
        else if (strcmp(arith, "sub") == 0)
        {
            result = i1 - i2;
            proto.arith = 2;
        }
        else if (strcmp(arith, "mul") == 0)
        {
            result = i1 * i2;
            proto.arith = 3;
        }
        else if (strcmp(arith, "div") == 0)
        {
            result = i1 / i2;
            proto.arith = 4;
        }

        proto.inValue1 = i1;
        proto.inValue2 = i2;
        proto.inResult = result;
        printf("Server -> Client[%d]: %d %s %d = %d\n", id, i1, arith, i2, result);
    }

    //Setting the up the rest of the protocol
    proto.id = id;
    proto.type = 2;
    proto.major_version = 1;
    proto.minor_version = 0;

    return proto;
}

calcMessage GetOKmsg()
{
    calcMessage msg;
    msg.type = htons(2);      //Server to client
    msg.protocol = htons(17); //UDP protocol
    msg.major_version = htons(1);
    msg.minor_version = htons(0);
    msg.message = htonl(1); //Okey message
    return msg;
}

calcMessage GetNOTOKmsg()
{
    calcMessage msg;
    msg.type = htons(2);      //Server to client
    msg.protocol = htons(17); //UDP protocol
    msg.major_version = htons(1);
    msg.minor_version = htons(0);
    msg.message = htonl(2); //Not okey...
    return msg;
}

size_t CompareProtocols(calcProtocol &client, calcProtocol &server, int id)
{
    int success = 1;

    //Int result check
    if (server.arith <= 4)
    {
        if (server.inResult != client.inResult)
        {
            success = -1;
        }
        printf("Server got: %d and Client[%d] got: %d\n", 
                server.inResult, id, client.inResult);
    }
    //Float result check
    else
    {
        if (server.flResult != client.flResult)
        {
            success = -1;
        }
        printf("Server got: %f and Client[%d] got: %f\n", 
                server.flResult, id, client.flResult);
    }

    return success;
}

//Hold everything useful for a client
struct ClientStruct
{
    sockaddr address;
    int id;
    calcProtocol serverAnswer;
};

//Check if the the two addresses are the same
int CompareAddr(sockaddr addr1, sockaddr addr2)
{
    int matched = 0;
    char address1[INET6_ADDRSTRLEN];
    char address2[INET6_ADDRSTRLEN];

    //Check if they are same types
    if (addr1.sa_family == addr2.sa_family)
    {
        //IPv4
        if (addr1.sa_family == AF_INET)
        {
            inet_ntop(AF_INET, &addr1, address1, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &addr2, address2, INET_ADDRSTRLEN);

            if (strcmp(address1, address2) == 0)
            {
                matched = TRUE;
            }
        }
        //IPv6
        else
        {
            inet_ntop(AF_INET6, &addr1, address1, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &addr2, address2, INET6_ADDRSTRLEN);

            if (strcmp(address1, address2) == 0)
            {
                matched = TRUE;
            }
        }
    }

    return matched;
}

//Check if the address exist in the queue. Return the index in queue
int InQueue(std::vector<ClientStruct> &queue, sockaddr address)
{
    int index = -1;
    int looping = TRUE;
    for (int i = 0; i < queue.size() && looping == TRUE; i++)
    {
        if (CompareAddr(queue[i].address, address) == TRUE)
        {
            index = i;
            looping = FALSE;
        }
    }
    return index;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Input should be './server' 'IP:PORT' \n");
        exit(1);
    }

    int sockfd;
    SetupServersocket(argv[1], sockfd);

    int id = 0;
    //Buffer for recieve
    char buffer[MSGMAXSIZE];

    //Holds the address to from recieve
    struct sockaddr clientAddr;          
    socklen_t addrSize = sizeof(clientAddr);
    size_t recvbytes = 0;
    std::vector<ClientStruct>clients;

    //Do not want to loop forever
    int looped = 0;
    int keepLooping = TRUE;

    //Keep looping as it is the server
    while (keepLooping == TRUE)
    {   
        //Recieving from some address
        recvbytes = recvfrom(sockfd, &buffer, sizeof(buffer), 0,
                            (struct sockaddr *)&clientAddr, &addrSize);

        //If it was a message
        if (recvbytes == sizeof(calcMessage))
        {
            //Reset, server still active
            looped = 0;

            int index = InQueue(clients, clientAddr);

            //If new client. Add the address
            if (index == -1)
            {
                ClientStruct newclient;
                newclient.address = clientAddr;
                newclient.id = id;
                id++;   //One new client
                clients.push_back(newclient);
                index = clients.size() - 1;
            }

            //Get whole struct of that client
            ClientStruct thisClient = clients.at(index);

            //Convert the buffer to message struct
            calcMessage clientMsg;
            memcpy(&clientMsg, buffer, sizeof(calcMessage));
            ConvertNetToHost(clientMsg);

            //Check if the server support that protocol
            if (clientMsg.type == 22 && clientMsg.message == 0 && 
                clientMsg.protocol == 17 &&
                clientMsg.major_version == 1 && clientMsg.minor_version == 0)
            {
                calcProtocol resultProto = GetNewProtocol(thisClient.id);
                thisClient.serverAnswer = resultProto;
                clients.at(index) = thisClient;

                //Prepare the protocol to send. Reset answers to avoid cheating
                calcProtocol serverProto = resultProto;
                serverProto.flResult = 0.0f;
                serverProto.inResult = 0;
                ConvertHostToNet(serverProto);
                SendCalcProto(sockfd, thisClient.address, serverProto, thisClient.id);
            }
            //Server does not support the protocol. Return not ok
            else
            {
                SendCalcMsg(sockfd, thisClient.address, GetNOTOKmsg(), thisClient.id);
            }
        }

        //Got a protocol
        else if (recvbytes == sizeof(calcProtocol))
        {
            int index = InQueue(clients, clientAddr);
            
            //Only support the ones in queue
            if (index != -1)
            {
                //Check the protocol with the answer

                calcProtocol clientProto;
                memcpy(&clientProto, buffer, sizeof(clientProto));
                ConvertNetToHost(clientProto);
                
                ClientStruct thisClient = clients.at(index);

                //ID from the protocol have to match the saved in vector
                if (clientProto.id == thisClient.id)
                {
                    calcProtocol result = thisClient.serverAnswer;

                    //Compare the clients protocol with the answer
                    if (CompareProtocols(clientProto, result, thisClient.id) != size_t(-1))
                    {
                        printf("Server -> Client[%d]: It was a match!\n", thisClient.id);
                        //Send back to the client that it worked
                        SendCalcMsg(sockfd, thisClient.address, GetOKmsg(), thisClient.id);
                        printf("\n");
                    }
                    //The two result did not match...
                    else
                    {
                        SendCalcMsg(sockfd, thisClient.address, GetNOTOKmsg(), thisClient.id);
                    }
                    //Remove it from the clients queue. Already solved
                    clients.erase(clients.begin()+index);
                }
                else
                {
                    printf("Error... ID's not matched...\n");
                }        
            }
            else
            {
                printf("Error, does not exist in queue...\n");
            }
        }
        // -1 from recv indicates error, 
        // which will happen if recvfrom takes longer than max recv time
        else if (recvbytes == size_t(-1))
        {
            //Clients respons took over 10 seconds...
            printf("Server have waited for 10 seconds...\n");
            if (!clients.empty())
            {
                printf("Removes all 'connected' clients\n");
                clients.clear();
            }
            printf("\n");

            looped++;

            //Reached maximum wait time
            if (looped == LOOPSWITHOUTRECV)
            {
                printf("Reached maximum wait time...\n");
                keepLooping = FALSE;
            }
        }
        else 
        {
            printf("Format not supported... Only takes message and protocol\n");
        }
    }

    printf("Closing down socket. Done for today\n");
    close(sockfd);
    return (0);
}
