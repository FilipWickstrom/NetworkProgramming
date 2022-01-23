#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <sys/socket.h>   //Socket
#include <sys/types.h>    //Socket
#include <arpa/inet.h>    //sockaddr
#include <string.h>       //Memset
#include <unistd.h>       //Close()

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

int main(int argc, char *argv[])
{  
    int sockFD;
    struct sockaddr_in serverAddress;
    struct calcMessage clientCalcMsg;
    struct calcMessage serverCalcMsg;
    struct calcProtocol serverCalcProto;
    struct calcProtocol clientCalcProto;
    socklen_t serverMsgLength = sizeof(serverCalcMsg);

    //Checks the amount of arguments
    if (argc < 3)
    {
        printf("***ERROR: Too few arguments...\nNeeds server-ip and port...***\n");
        exit(0);
    }

    //Creates the socket
    sockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFD < 0)
    {
        printf("***ERROR: Failed to create socket...***\n");
        exit(0);
    }
    else
    {
        printf("+ Client socket created\n");
    }
    

    //Resets and setup serveraddress
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;                     //IPv4
    serverAddress.sin_addr.s_addr = inet_addr(argv[1]);     //Sets the address from input
    serverAddress.sin_port = htons(atoi(argv[2]));          //Sets the port from input
        
    //Setups the first clientmessage with protocol
    clientCalcMsg.type = 22;
    clientCalcMsg.message = 0;
    clientCalcMsg.protocol = 17;
    clientCalcMsg.major_version = 1;
    clientCalcMsg.minor_version = 0;
    
    //Sends the message (with info above)
    if (sendto(sockFD, &clientCalcMsg, sizeof(clientCalcMsg), 0, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        printf("***ERROR: Client failed to send message...***\n");
        exit(0);
    } 
    else
    {
        printf("CLIENT: Sent protocol\n");
    }

    //Recieves message about the protocolcheck
    if (recvfrom(sockFD, &serverCalcMsg, sizeof(serverCalcMsg), 0, (struct sockaddr*) &serverAddress, &serverMsgLength) < 0)
    {
        printf("***ERROR: Failed to read message...***\n");
        exit(0);
    }
    else
    {
        //When server send this message it time to die...
        if (serverCalcMsg.type==2 && serverCalcMsg.message==2 && serverCalcMsg.major_version==1 && serverCalcMsg.minor_version==0)
        {
            printf("SERVER: Do not support that protocol...\nShutting down...\n");
            close(sockFD);
            exit(0);
        }
        else
        {
            printf("SERVER: Protocol OK!\n");
        }   
    }

    
    
    //STOPS HERE*****************************************************
    
    //Recieves the calcProtocol from server
    if (recvfrom(sockFD, &serverCalcProto, sizeof(serverCalcProto), 0, (struct sockaddr*) &serverAddress, &serverMsgLength) < 0)
    {
        printf("***ERROR: Failed to read message...***\n");
        exit(0);
    }

    else
    {
        printf("RECIEVED MESSAGE\n");
        //Moving and setting values for the clients calcProtocol
        clientCalcProto.type = 2;
        clientCalcProto.major_version = 1;
        clientCalcProto.minor_version = 0;
        clientCalcProto.id = serverCalcProto.id;
        clientCalcProto.arith = serverCalcProto.arith;

        if (clientCalcProto.arith <= 4)         //add/sub/mul/div
        {
            clientCalcProto.inValue1 = serverCalcProto.inValue1;
            clientCalcProto.inValue2 = serverCalcProto.inValue2;
            if (clientCalcProto.arith == 1)         //add
            {
                clientCalcProto.inResult = clientCalcProto.inValue1 + clientCalcProto.inValue2;
                printf("CLIENT: %d + %d = %d\n", clientCalcProto.inValue1, clientCalcProto.inValue2, clientCalcProto.inResult);
            }
            else if (clientCalcProto.arith == 2)    //sub
            {
                clientCalcProto.inResult = clientCalcProto.inValue1 - clientCalcProto.inValue2;
                printf("CLIENT: %d - %d = %d\n", clientCalcProto.inValue1, clientCalcProto.inValue2, clientCalcProto.inResult);
            }
            else if (clientCalcProto.arith == 3)    //mul
            {
                clientCalcProto.inResult = clientCalcProto.inValue1 * clientCalcProto.inValue2;
                printf("CLIENT: %d * %d = %d\n", clientCalcProto.inValue1, clientCalcProto.inValue2, clientCalcProto.inResult);
            }
            else if (clientCalcProto.arith == 4)    //div
            {
                clientCalcProto.inResult = clientCalcProto.inValue1 / clientCalcProto.inValue2;
                printf("CLIENT: %d / %d = %d\n", clientCalcProto.inValue1, clientCalcProto.inValue2, clientCalcProto.inResult);
            }
            printf("SERVER: %d\n", serverCalcProto.inResult);
            
            //Checks the result
            if (clientCalcProto.inResult == serverCalcProto.inResult)
            {
                printf("CLIENT: OK! Results matched\n");
                clientCalcMsg.type = 22;
                clientCalcMsg.message = 1;  //OKEY
            }
            else
            {
                printf("CLIENT: NOT OKEY! Result not matched...\n");
                clientCalcMsg.type = 22;
                clientCalcMsg.message = 2;  //NOT OKEY
            }
        }
        else        //fadd/fsub/fmul/fdiv
        {
            clientCalcProto.flValue1 = serverCalcProto.flValue1;
            clientCalcProto.flValue2 = serverCalcProto.flValue2;
            if (clientCalcProto.arith == 5)         //fadd
            {
                clientCalcProto.flResult = clientCalcProto.flValue1 + clientCalcProto.flValue2;
                printf("CLIENT: %8.8g + %8.8g = %8.8g\n", clientCalcProto.flValue1, clientCalcProto.flValue2, clientCalcProto.flResult);
            }
            else if (clientCalcProto.arith == 6)    //fsub
            {
                clientCalcProto.flResult = clientCalcProto.flValue1 - clientCalcProto.flValue2;
                printf("CLIENT: %8.8g - %8.8g = %8.8g\n", clientCalcProto.flValue1, clientCalcProto.flValue2, clientCalcProto.flResult);
            }
            else if (clientCalcProto.arith == 7)    //fmul
            {
                clientCalcProto.flResult = clientCalcProto.flValue1 * clientCalcProto.flValue2;
                printf("CLIENT: %8.8g * %8.8g = %8.8g\n", clientCalcProto.flValue1, clientCalcProto.flValue2, clientCalcProto.flResult);
            }
            else if (clientCalcProto.arith == 8)    //fdiv
            {
                clientCalcProto.flResult = clientCalcProto.flValue1 / clientCalcProto.flValue2;
                printf("CLIENT: %8.8g / %8.8g = %8.8g\n", clientCalcProto.flValue1, clientCalcProto.flValue2, clientCalcProto.flResult);
            }
            printf("SERVER: %8.8g\n", serverCalcProto.flResult);
            double delta = abs(clientCalcProto.flResult - serverCalcProto.flResult);
            double epsilon = 0.001;

            if (delta < epsilon)
            {
                printf("CLIENT: OK! Results matched\n");
                clientCalcMsg.type = 22;
                clientCalcMsg.message = 1;  //OKEY
            }
            else
            {
                printf("CLIENT: NOT OKEY! Result not matched...\n");
                clientCalcMsg.type = 22;
                clientCalcMsg.message = 2;  //NOT OKEY
            }
        }

        //Skickar tillbaka ett calcMessage med OK eller NOT OK
        if (sendto(sockFD, &clientCalcMsg, sizeof(clientCalcMsg), 0, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        {
            printf("***ERROR: Client failed to send message...***\n");
        } 
        else
        {
            printf("CLIENT: Sent calcProtocol to server\n");
        }
        

    }

    

    close(sockFD);
    return 0;
}
