CC = g++
CFLAGS = -g -std=c++11 -Wall -pedantic

all: server client

server: messenger_server.cpp 
	$(CC) $(CFLAGS) messenger_server.cpp -o messenger_server

client: messenger_client.cpp 
	$(CC) $(CFLAGS) messenger_client.cpp -o messenger_client -lpthread

clean: 
	$(RM) messenger_server messenger_client