#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

/* You will to add includes here */
#include <sys/socket.h>   //Socket
#include <sys/types.h>    //Socket
#include <arpa/inet.h>    //For sockaddr
#include <string.h>       //Memset
//#include <netinet/in.h>
//#include <netdb.h>

// Included to get the support library
#include <calcLib.h>
#include "protocol.h"

#define MAXBUF 512

/* Needs to be global, to be rechable by callback and main */
int loopCount=0;
int terminate=0;

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobbList(int signum)
{
    // As anybody can call the handler, its good coding to check the signal number that called it.

    printf("Let me be, I want to sleep.\n");

    if(loopCount>20){
        printf("I had enough.\n");
        terminate=1;
    }
    
    return;
}

int main(int argc, char *argv[])
{  
    int sockFD;
    int bindFD;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    struct calcMessage serverCalcMsg;
    struct calcMessage clientCalcMsg;
    struct calcProtocol serverCalcProto;
    socklen_t clientAddrLength = sizeof(clientAddress);
    
    //Checks if the user have written the port
    if (argc < 2)
    {
        printf("***ERROR: Missed to write the serverport...***\n");
        exit(0);
    }

    //Creates a UDP-socket with IPv4 connection
    sockFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);    
    if (sockFD < 0)
    {
        printf("***ERROR: Failed to create server...***\n");
        exit(0);
    }
    else
    {
        printf("+ Server socket created\n");
    }
    
    //Setup serverinformation
    memset(&clientAddress, 0, sizeof(clientAddress));
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;                 //IPv4
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);  //Sets the ip-address
    serverAddress.sin_port = htons(atoi(argv[1]));      //Port from input

    //Binds socket to address
    bindFD = bind(sockFD, (const struct sockaddr*) &serverAddress, sizeof(serverAddress));
    if (bindFD < 0)
    {
        printf("***ERROR: Failed to bind socket...***\n");
        exit(0);
    }
    else
    {
        printf("+ Server socket binded\n");
    }
    

    /* 
      Prepare to setup a reoccurring event every 10s. If it_interval, or it_value is omitted, it will be a single alarm 10s after it has been set. 
    */
    struct itimerval alarmTime;
    alarmTime.it_interval.tv_sec=10;
    alarmTime.it_interval.tv_usec=10;
    alarmTime.it_value.tv_sec=10;
    alarmTime.it_value.tv_usec=10;

    /* Regiter a callback function, associated with the SIGALRM signal, which will be raised when the alarm goes of */
    signal(SIGALRM, checkJobbList);
    setitimer(ITIMER_REAL,&alarmTime,NULL); // Start/register the alarm. 

    //No need to accept anything
    while(terminate==0)
    {
        //printf("\n-------    Client connected    -------\n");     *********
        
        //Recieves a calcMessage from client with protocol
        if (recvfrom(sockFD, &clientCalcMsg, sizeof(clientCalcMsg), 0, (struct sockaddr*)&clientAddress, &clientAddrLength) < 0)
        {
            printf("***ERROR: Could not recieve first message...***\n");
        }
        else
        {
            printf("CLIENT: Sent protocol\n");
        }      
  
        //Checks the protocol
        if (clientCalcMsg.protocol != 17)
        {
            printf("***ERROR: Wrong protocol***\n");
            serverCalcMsg.type = 2;
            serverCalcMsg.message = 2;
            serverCalcMsg.major_version = 1;
            serverCalcMsg.minor_version = 0;
            if (sendto(sockFD, &serverCalcMsg, sizeof(serverCalcMsg), 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0)
            {
                printf("***ERROR: Failed to send 'wrong protocol' message***\n");
            }
        }
        else
        {
            printf("SERVER: Protocol OK!\n");
            //Setups the calcProtocol
            serverCalcProto.type = 1;
            serverCalcProto.major_version = 1;
            serverCalcProto.minor_version = 0;
            serverCalcProto.id = 9;
            serverCalcProto.arith = rand() % 8 + 1;     //Should I use calcLib.c instead somehow??????
            
            if (serverCalcProto.arith <= 4)     //Checks add/sub/mul/div
            {
                serverCalcProto.inValue1 = randomInt();
                serverCalcProto.inValue2 = randomInt();

                if (serverCalcProto.arith == 1)         //add
                {
                    serverCalcProto.inResult = serverCalcProto.inValue1 + serverCalcProto.inValue2;
                    printf("SERVER: %d + %d = %d\n", serverCalcProto.inValue1, serverCalcProto.inValue2, serverCalcProto.inResult);
                }
                else if (serverCalcProto.arith == 2)    //sub
                {
                    serverCalcProto.inResult = serverCalcProto.inValue1 - serverCalcProto.inValue2;
                    printf("SERVER: %d - %d = %d\n", serverCalcProto.inValue1, serverCalcProto.inValue2, serverCalcProto.inResult);
                }
                else if (serverCalcProto.arith == 3)    //mul
                {
                    serverCalcProto.inResult = serverCalcProto.inValue1 * serverCalcProto.inValue2;
                    printf("SERVER: %d * %d = %d\n", serverCalcProto.inValue1, serverCalcProto.inValue2, serverCalcProto.inResult);
                }
                else if (serverCalcProto.arith == 4)    //div
                {
                    serverCalcProto.inResult = serverCalcProto.inValue1 / serverCalcProto.inValue2;
                    printf("SERVER: %d / %d = %d\n", serverCalcProto.inValue1, serverCalcProto.inValue2, serverCalcProto.inResult);
                }
            }
            else        //Checks fadd/fsub/fmul/fdiv
            {
                serverCalcProto.flValue1 = randomFloat();
                serverCalcProto.flValue2 = randomFloat();

                if (serverCalcProto.arith == 5)         //fadd
                {
                    serverCalcProto.flResult = serverCalcProto.flValue1 + serverCalcProto.flValue2;
                    printf("SERVER: %8.8g + %8.8g = %8.8g\n", serverCalcProto.flValue1, serverCalcProto.flValue2, serverCalcProto.flResult);
                }
                else if (serverCalcProto.arith == 6)    //fsub
                {
                    serverCalcProto.flResult = serverCalcProto.flValue1 - serverCalcProto.flValue2;
                    printf("SERVER: %8.8g - %8.8g = %8.8g\n", serverCalcProto.flValue1, serverCalcProto.flValue2, serverCalcProto.flResult);
                }
                else if (serverCalcProto.arith == 7)    //fmul
                {
                    serverCalcProto.flResult = serverCalcProto.flValue1 * serverCalcProto.flValue2;
                    printf("SERVER: %8.8g * %8.8g = %8.8g\n", serverCalcProto.flValue1, serverCalcProto.flValue2, serverCalcProto.flResult);
                }
                else if (serverCalcProto.arith == 8)    //fdiv
                {
                    serverCalcProto.flResult = serverCalcProto.flValue1 / serverCalcProto.flValue2;
                    printf("SERVER: %8.8g / %8.8g = %8.8g\n", serverCalcProto.flValue1, serverCalcProto.flValue2, serverCalcProto.flResult);
                }
            }
            
            //Sends this protocol to the client
            if (sendto(sockFD, &serverCalcProto, sizeof(serverCalcProto), 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0)
            {
                printf("***ERROR: Failed to send the calcProtocol***\n");
            }
            else
            {
                printf("SERVER: Sent calcProtocol\n");

            }

            //Recieves the result
            if (recvfrom(sockFD, &clientCalcMsg, sizeof(clientCalcMsg), 0, (struct sockaddr*)&clientAddress, &clientAddrLength) < 0)
            {
                printf("***ERROR: Could not recieve first message...***\n");
            }
            else
            {
                if (clientCalcMsg.type == 22 && clientCalcMsg.message == 1)
                {
                    printf("CLIENT: OK! Results matched\n");
                }
                else
                {
                    printf("CLIENT: NOT OKEY! Result not matched...\n");
                }
            }
            
            
        }
        
        
        
        //printf("This is the main loop, %d time.\n",loopCount);
        sleep(1);
        loopCount++;
    }

    printf("done.\n");
    close(sockFD);
    return(0);
}

/*
1. create socket
2. bind()
3. recvfrom()
   blocks until datagram recieved from client
4. sendto()
*/
