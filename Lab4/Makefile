PP = g++
CC_FLAGS = -w -g

all: client server

client: client.o sspLib.o 
	$(PP) -Wall -o sspgame client.o sspLib.o

server: server.o sspLib.o
	$(PP) -Wall -o sspd server.o sspLib.o -lpthread

clean:
	rm *.o sspd sspgame
