CC = gcc
CFLAGS = -Wall -pthread

all: server/server

server/server: server/server.c protocol/protocol.h
	$(CC) $(CFLAGS) -o server/server server/server.c
	
clean:
	rm -f server/server
