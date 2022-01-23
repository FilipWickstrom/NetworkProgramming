#include <stdio.h>
#include <stdlib.h>

/* You will to add includes here */
#include <sys/socket.h> //Socket
#include <sys/types.h>  //Socket
#include <string.h>     //For memset
#include <unistd.h>     //For close()
#include <netinet/in.h> //For INADDR_ANY

// Included to get the support library
#include <calcLib.h>
#include <calcLib.c>

#define BUF_SIZE 256     //block transfer size
#define QUEUE_SIZE 5

int initCalcLib(void);
int randomInt(void);
double randomFloat(void);
char* randomType(void);

int main(int argc, char *argv[])
{  
    if (argc < 2)
    {
        printf("ERROR: Missed to write the serverport...\n");
        exit(0);
    }

    initCalcLib();
    int createSocket;
    int bindSocket; 
    int listenSocket;
    int socketAccept; 
    struct sockaddr_in serverAddress;   //Receive from this address
    struct sockaddr_in clientAddress;   //Sends to this address
    socklen_t clientAddressLength = sizeof(clientAddress);
    int sendMsg;
    int recieveMsg;
    char clientmsg[BUF_SIZE];
    char servermsg[BUF_SIZE];
    char protocol[20] = "TEXT TCP 1.0\n";

    memset(&serverAddress, 0, sizeof(serverAddress));   //Reset 
    serverAddress.sin_family = AF_INET;                 //IPv4
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(atoi(argv[1]));      //Port from input

    //Creates server socket
    createSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   //Socket with IPv4 och TCP-protocol
    if (createSocket < 0)
    {
        printf("ERROR: Socket could not be created...\n");
        close(createSocket);
        exit(0);
    }
    else 
    {
        printf("+ Server socket created\n");
    }

    //Binds the socket to the address
    bindSocket = bind(createSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (bindSocket < 0)
    {
        printf("ERROR: Socket could not bind...\n");
        close(createSocket);
        exit(0);
    }
    else
    {
        printf("+ Socket binded to address\n");
    }
    
    //Listen to a maximum of 5 clients
    listenSocket = listen(createSocket, QUEUE_SIZE);
    if (listenSocket < 0)
    {
        printf("ERROR: Socket could not listen...\n");
        close(createSocket);
        exit(0);
    }
    else
    {
        printf("+ Listening for client on port: %d\n", ntohs(serverAddress.sin_port));
    }
     
    //Waiting for client/clients to connect to the server.
    while (true)
    {
        socketAccept = accept(createSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (socketAccept < 0)
        {
            printf("ERROR: Socket could not accept...\n");
        }
        else 
        {
            printf("\n------  Client connected to server  ------\n");
            //Sending a message to check if it is the right protocol
            sendMsg = send(socketAccept, protocol, strlen(protocol), 0);
            if (sendMsg < 0)
            {
                printf("ERROR: Could not send message...\n");
            }
            printf("SERVER: %s", protocol);
        }

        memset(clientmsg, 0, sizeof(clientmsg));   //Reset clientmsg so it is not anything there from last time
        //Recieves a message from client if the protocol was okey or not
        recieveMsg = recv(socketAccept, clientmsg, BUF_SIZE, 0);
        if (recieveMsg < 0)
        {
            printf("ERROR: Could not recieve message...\n");
            shutdown(socketAccept, SHUT_RDWR);
        }
        else
        {
            printf("CLIENT: %s", clientmsg);
        }

        //- - - - - - -     Arithmetics     - - - - - - -
        
        char* typePtr = randomType();     //Gives a random add/div/.../fadd/...
        int iValue1, iValue2;
        double fValue1, fValue2, result, resultFromClient;
        
        //Randomizes values and calculates the result (for server)
        if (typePtr[0] == 'f')
        {
            fValue1 = randomFloat();
            fValue2 = randomFloat();
            sprintf(servermsg, "%s %8.8g %8.8g\n", typePtr, fValue1, fValue2);
            printf("SERVER: %s", servermsg);
            
            if (strcmp(typePtr,"fadd") == 0)
                result = fValue1 + fValue2;
            else if (strcmp(typePtr, "fsub") == 0)
                result = fValue1 - fValue2;
            else if (strcmp(typePtr, "fmul") == 0)
                result = fValue1 * fValue2;
            else if (strcmp(typePtr, "fdiv") == 0)
                result = fValue1 / fValue2;
            printf("SERVER: result: %8.8g\n", result);
        }
        else
        {
            iValue1 = randomInt();
            iValue2 = randomInt();
            sprintf(servermsg, "%s %d %d\n", typePtr, iValue1, iValue2);
            printf("SERVER: %s", servermsg);
            
            if (strcmp(typePtr,"add") == 0)
                result = iValue1 + iValue2;
            else if (strcmp(typePtr, "sub") == 0)
                result = iValue1 - iValue2;
            else if (strcmp(typePtr, "mul") == 0)
                result = iValue1 * iValue2;
            else if (strcmp(typePtr, "div") == 0)
                result = iValue1 / iValue2;
            printf("SERVER: result: %d\n", (int)result);
        }

        //Sends a string with the "commands" and values
        sendMsg = send(socketAccept, servermsg, strlen(servermsg), 0);
        if (sendMsg < 0)
        {
            printf("ERROR: Could not send message...\n");
        }

        //Reads the result from the client
        memset(clientmsg, 0, sizeof(clientmsg));
        recieveMsg = recv(socketAccept, clientmsg, sizeof(clientmsg), 0);
        if (recieveMsg < 0)
        {
            printf("ERROR: Could not send message...\n");
        }
        
        //Saves the clientmessage to a double in variabel resultFromClient
        sscanf(clientmsg, "%lf", &resultFromClient);

        //Comparison between server- and clientresult
        if (typePtr[0] == 'f')
        {
            printf("CLIENT: result: %8.8g\n", resultFromClient);
            double delta = abs(result - resultFromClient);
            double epsilon = 0.001;
            
            //Will be right if there is not much differens between the two results
            if (delta < epsilon)
            {
                sprintf(servermsg, "%s", "OK! Results matched\n");
                printf("SERVER: %s", servermsg);
            }
            else
            {
                sprintf(servermsg, "%s", "ERROR! Results did not match...\n");
                printf("SERVER: %s", servermsg);
            }
        }
        else
        {
            printf("CLIENT: result: %d\n", (int)resultFromClient);
            //Checks the int results
            if ((int)result == (int)resultFromClient)
            {
                sprintf(servermsg, "%s", "OK! Results matched\n");
                printf("SERVER: %s", servermsg);
            }
            else
            {
                sprintf(servermsg, "%s", "ERROR! Results did not match...\n");
                printf("SERVER: %s", servermsg);
            }
        }
                
        //Sends back OK! or ERROR!
        sendMsg = send(socketAccept, servermsg, strlen(servermsg), 0);
        if (sendMsg < 0)
        {
            printf("ERROR: Could not send message...\n");
        }

        shutdown(socketAccept, SHUT_RDWR);
        close(socketAccept);
        printf("------  Client left server  ------\n");
    }
    close(createSocket);
    return 0;
}
