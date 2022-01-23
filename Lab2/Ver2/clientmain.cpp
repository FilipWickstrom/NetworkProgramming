#include <stdio.h>
#include <stdlib.h>

// My includes
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h> //structs
#include <string.h>    //memset
#include <unistd.h>    //close()

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

/*
UDP client that sends binary
Programmed by Filip Wickström
Last update: 2021-02-15
*/

// Easier to change and understand
#define MSGMAXSIZE 50
#define TIMEOUTSEC 2
#define MAXTRIES 3
#define ADD 1
#define SUB 2
#define MUL 3
#define DIV 4
#define FADD 5
#define FSUB 6
#define FMUL 7
#define FDIV 8

// Helpfunctions
void SetupSocket(int &sockFd, struct sockaddr_in &addr, char* ipPort)
{
    unsigned short int ip[4] = {0};
    unsigned short int port = 0;
    //Check that the input works. All 5 variables should be filled
    if (sscanf(ipPort, "%hu%*c%hu%*c%hu%*c%hu%*c%hu", &ip[0], &ip[1], &ip[2], &ip[3], &port) != 5)
    {
        printf("Incorrect input...\nTry: '<IP>:<PORT>'\n");
        exit(1);    //Abnormal termination
    }

    //Max length is 15 characters. 12 for numbers + 3 dots
    char fullIp[15];
    sprintf(fullIp, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    //Create clients socket
    sockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockFd < 0)
    {
        printf("Failed to create socket...\n");
        exit(1);
    }
    else
    {
        printf("+Client socket created\n");
    }

    //Information about the server we are going to send to
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; //IPv4
    addr.sin_addr.s_addr = inet_addr(fullIp);
    addr.sin_port = htons(port);

    //Setting up so that the socket can only recieve input for a maximum time
    struct timeval timer;
    timer.tv_sec = TIMEOUTSEC;
    timer.tv_usec = 0;
    if (setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (timeval*)&timer, sizeof(timeval)) == -1)
    {
        printf("Failed to setup sockets options...\n");
        close(sockFd);
        exit(1);
    }
}

calcMessage GetFirstMessage()
{
    calcMessage msg;
    msg.type = htons(22);           //Client to server
    msg.message = htonl(0);         //not availible
    msg.protocol = htons(17);       //UDP
    msg.major_version = htons(1);
    msg.minor_version = htons(0);
    return msg;
}

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

void ConvertNetToHost(calcProtocol &protocol)
{
    //Short - ntohs conversion
    protocol.type = ntohs(protocol.type);
    protocol.major_version = ntohs(protocol.major_version);
    protocol.minor_version = htons(protocol.minor_version);

    //Long - ntohl conversion
    protocol.arith = ntohl(protocol.arith);
    protocol.id = ntohl(protocol.id);
    protocol.inValue1 = ntohl(protocol.inValue1);
    protocol.inValue2 = ntohl(protocol.inValue2);
    protocol.inResult = ntohl(protocol.inResult);

    //Do nothing with the "floats"
}

void ConvertHostToNet(calcProtocol &protocol)
{
    //Short - htons conversion
    protocol.type = htons(protocol.type);
    protocol.major_version = htons(protocol.major_version);
    protocol.minor_version = htons(protocol.minor_version);

    //Long - htonl conversion
    protocol.arith = htonl(protocol.arith);
    protocol.id = htonl(protocol.id);
    protocol.inValue1 = htonl(protocol.inValue1);
    protocol.inValue2 = htonl(protocol.inValue2);
    protocol.inResult = htonl(protocol.inResult);

    //Do nothing with the "floats"
}

calcProtocol CalcTheProtocol(calcProtocol server)
{
    calcProtocol client;
    client.type = 2;
    client.major_version = 1;
    client.minor_version = 0;
    client.id = server.id;
    client.arith = server.arith;
    char arith[4] = {'+', '-', '*', '/'};

    //Int calculation
    if (client.arith <= DIV)
    {
        client.inValue1 = server.inValue1;
        client.inValue2 = server.inValue2;

        if (client.arith == ADD)
        {
            client.inResult = client.inValue1 + client.inValue2;
        }
        else if (client.arith == SUB)
        {
            client.inResult = client.inValue1 - client.inValue2;
        }
        else if (client.arith == MUL)
        {
            client.inResult = client.inValue1 * client.inValue2;
        }
        else if (client.arith == DIV)
        {
            client.inResult = client.inValue1 / client.inValue2;
        }
        //Printing out what it calculated
        printf("Client: %d %c %d = %d\n", client.inValue1, arith[client.arith-1], client.inValue2, client.inResult);
    }

    //Float calculation
    else
    {
        client.flValue1 = server.flValue1;
        client.flValue2 = server.flValue2;

        if (client.arith == FADD)
        {
            client.flResult = client.flValue1 + client.flValue2;
        }
        else if (client.arith == FSUB)
        {
            client.flResult = client.flValue1 - client.flValue2;
        }
        else if (client.arith == FMUL)
        {
            client.flResult = client.flValue1 * client.flValue2;
        }
        else if (client.arith == FDIV)
        {
            client.flResult = client.flValue1 / client.flValue2;
        }

        printf("Client: %f %c %f = %f\n", client.flValue1, arith[(client.arith-1) % 4], client.flValue2, client.flResult);
    }

    return client;
}

// Main function
int main(int argc, char *argv[])
{
    //Input needs to be atleast two parts. "./client" and "<ip>:<port>"
    if (argc > 1)
    {
        int sockFd;
        struct sockaddr_in serverAddr;

        //Setting up the socket, server address from the input
        SetupSocket(sockFd, serverAddr, argv[1]);

        //First message with UDP - client to server
        calcMessage firstMsg = GetFirstMessage();

        //Max size of what the server is sending is the size of protocol size
        char msgCharArray[MSGMAXSIZE];
        socklen_t msgLength = MSGMAXSIZE;
        calcMessage serverMsg;
        calcProtocol serverProto;
        
        //Reset values to zero
        memset(&serverMsg, 0, sizeof(calcMessage));
        memset(&serverProto, 0, sizeof(calcProtocol));
        memset(&msgCharArray, 0, MSGMAXSIZE);

        //Number of tries to send the message
        int sendTries = 0;
        int success = 0;
        size_t sendBytes = 0;
        size_t recvBytes = 0;

        //First send to the server - has 3 tries to send and receive
        while (success == 0 && sendTries < MAXTRIES)
        {
            //Sending the message to the server
            sendBytes = sendto(sockFd, &firstMsg, sizeof(firstMsg), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
            if (sendBytes < 0)
            {
                printf("Failed to send message...\n");
            }
            else
            {
                printf("Client: sending message to server\n");

                //Recieving the message from the server
                recvBytes = recvfrom(sockFd, &msgCharArray, sizeof(msgCharArray), 0, (struct sockaddr *)&serverAddr, &msgLength);

                //Can recieve -1 if it was an error
                if (recvBytes != size_t(0) && recvBytes != size_t(-1))
                {
                    success = 1;
                }
            }
            sendTries++;

            //Check if we reached maximum number of tries
            if (sendTries == MAXTRIES)
            {
                printf("Client: Server not responding after %d tries...\nClosing down...\n", sendTries);
                close(sockFd);
                exit(1);
            }
        }

        //Check if the server sent us a message
        if (recvBytes == sizeof(calcMessage))
        {
            printf("Server: Sent a message\n");
            memcpy(&serverMsg, msgCharArray, sizeof(calcMessage));

            //Convert the message to readable values
            ConvertNetToHost(serverMsg);

            //Checking if the message that was returned was "NOT OK!"
            if (serverMsg.type == 2 && serverMsg.message == 2 &&
                serverMsg.major_version == 1 && serverMsg.minor_version == 0)
            {
                printf("Client: Recieved from server 'NOT OK!'\n");
                close(sockFd);
                exit(1);
            }
        }

        //Check to see if the server sent us a protocol to calculate
        else if (recvBytes == sizeof(calcProtocol))
        {
            printf("Server: Sent a protocol\n");
            memcpy(&serverProto, msgCharArray, sizeof(calcProtocol));
            ConvertNetToHost(serverProto);

            calcProtocol clientProto = CalcTheProtocol(serverProto);

            //Preparation for sending to the server
            ConvertHostToNet(clientProto);

            //Reset values for repeating if needed
            sendTries = 0;
            success = 0;
            memset(&serverMsg, 0, sizeof(calcMessage));

            //Sending more than one message to the server if it does not respond in time
            while (success == 0 && sendTries < MAXTRIES)
            {
                //Send the protocol to the server
                sendBytes = sendto(sockFd, &clientProto, sizeof(clientProto), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
                if (sendBytes < 0)
                {
                    printf("Failed to send protocol...\n");
                }
                else
                {
                    printf("Client: sending protocol to server\n");

                    //Recieving a message from the server
                    recvBytes = recvfrom(sockFd, &serverMsg, sizeof(serverMsg), 0, (struct sockaddr *)&serverAddr, &msgLength);

                    if (recvBytes != size_t(0) && recvBytes != size_t(-1))
                    {
                        success = 1;

                        //Converting the message data
                        ConvertNetToHost(serverMsg);

                        //Finally checking the message what it responded with
                        if (serverMsg.type == 2 && serverMsg.major_version == 1 &&
                            serverMsg.minor_version == 0)
                        {
                            if (serverMsg.message == 1)
                            {
                                printf("Server: OK!\n");
                            }
                            else if (serverMsg.message == 2)
                            {
                                printf("Server: NOT OK!\n");
                            }
                        }
                    }
                }
                sendTries++;

                //Check if we reached maximum number of tries
                if (sendTries == MAXTRIES)
                {
                    printf("Client: Server not responding after %d tries...\nClosing down...\n", sendTries);
                    close(sockFd);
                    exit(1);
                }
            }
        }

        //Closing down the socket
        printf("Client: Closing down...\n");
        close(sockFd);
    }
    else
    {
        printf("Empty input does not work...\nTry: '<IP>:<PORT>'\n");
    }
}
