#include <stdio.h>
#include <stdlib.h>

/* You will to add includes here */
#include <sys/socket.h> //Socket
#include <sys/types.h>  //Socket
#include <string.h>     //For memset
#include <unistd.h>     //For close()
#include <arpa/inet.h>

// Included to get the support library
#include <calcLib.h>

#define BUF_SIZE 256

int main(int argc, char *argv[])
{
    int createSocket;
    int connectSocket;
    int sendMsg;
    int recieveMsg;
    char servermsg[BUF_SIZE];
    char clientmsg[BUF_SIZE];
    struct sockaddr_in serverAddress;
    char protocol[20] = "TEXT TCP 1.0\n";

    //Checks if you have written atleast 3 "words", EX) ./client 0.0.0.0 2020 
    if (argc < 3)
    {
        printf("ERROR: Not enough arguments... Server-ip and port required\n");
    }
    else
    {
        //Creates the socket
        createSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (createSocket < 0)
        {
            printf("ERROR: Failed to create socket...\n");
            exit(0);
        }
        else
        {
            printf("+ Client socket created\n");
        }
        
        //Setups the address of the server
        memset(&serverAddress, 0, sizeof(serverAddress));       //Reset
        serverAddress.sin_family = AF_INET;                     //IPv4
        serverAddress.sin_addr.s_addr = inet_addr(argv[1]);     //Address from input
        serverAddress.sin_port = htons(atoi(argv[2]));          //Port from input 

        //Connects the socket to the server-address
        connectSocket = connect(createSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
        if (connectSocket < 0)
        {
            printf("ERROR: Failed to connect to server...\n");
            exit(0);
        }
        else
        {
            printf("+ Socket connected\n"); 
        }
        
        //Recieves a message from server with the protocol
        memset(servermsg, 0, sizeof(servermsg));
        recieveMsg = recv(createSocket, &servermsg, sizeof(servermsg), 0); 
        if (recieveMsg < 0)
        {
            printf("ERROR: Could not read the servers message...\n");
        }

        //Compares the protocols
        if (strcmp(servermsg, protocol) == 0)
        {
            printf("\n------   Connected to server ------\n");
            printf("SERVER: %s",servermsg);

            //Sends a message to the server that everything is okey
            sprintf(clientmsg, "%s", "OK! Accepted protocol\n");
            sendMsg = send(createSocket, clientmsg, strlen(clientmsg), 0);
            if (sendMsg < 0)
            {
                printf("ERROR: Failed to send message...\n");
            }
            printf("CLIENT: %s", clientmsg);

            //- - - - - - -     Arithmetics     - - - - - - -
            
            char currentOperator[5];
            int iValue1, iValue2;
            double fValue1, fValue2, result;

            //Recieves a message from server with the 'command' and values
            memset(servermsg, 0, sizeof(servermsg));
            recieveMsg = recv(createSocket, &servermsg, sizeof(servermsg), 0); 
            if (recieveMsg < 0)
            {
                printf("ERROR: Could not read the servers message...\n");
            }
            printf("SERVER: %s", servermsg);
    
            //Client makes the math
            if (servermsg[0] == 'f')
            {
                sscanf(servermsg, "%s %lf %lf", currentOperator, &fValue1, &fValue2);   //%lf is for double
                if (strcmp(currentOperator, "fadd") == 0)
                    result = fValue1 + fValue2;
                else if (strcmp(currentOperator, "fsub") == 0)
                    result = fValue1 - fValue2;
                else if (strcmp(currentOperator, "fmul") == 0)
                    result = fValue1 * fValue2;
                else if (strcmp(currentOperator, "fdiv") == 0)
                    result = fValue1 / fValue2;
                sprintf(clientmsg, "%8.8g\n", result);
            }   
            else
            {
                sscanf(servermsg, "%s %d %d", currentOperator, &iValue1, &iValue2);
                if (strcmp(currentOperator, "add") == 0)
                    result = iValue1 + iValue2;
                else if (strcmp(currentOperator, "sub") == 0)
                    result = iValue1 - iValue2;
                else if (strcmp(currentOperator, "mul") == 0)
                    result = iValue1 * iValue2;
                else if (strcmp(currentOperator, "div") == 0)
                    result = iValue1 / iValue2;
                sprintf(clientmsg, "%d\n", (int)result);
            }
            printf("CLIENT: result: %s", clientmsg);

            //Sends the clients result to the server
            sendMsg = send(createSocket, clientmsg, strlen(clientmsg), 0);
            if (sendMsg < 0)
            {
                printf("ERROR: Failed to send message...\n");
            }

            //Recieves a message from the server and tells if it was OK or ERROR
            memset(servermsg, 0, sizeof(servermsg));
            recieveMsg = recv(createSocket, &servermsg, sizeof(servermsg), 0);
            if (recieveMsg < 0)
            {
                printf("ERROR: Failed to recieve message...\n");
            }
            printf("SERVER: %s", servermsg);

            close(createSocket);
            printf("------   Left server  ------\n");

        }
        else
        {
            printf("ERROR: Client protocol did not match server protocol");
            exit(0);
        }
    }
    return 0;
}
