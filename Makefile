CC = gcc
CFLAGS = -Wall -pthread

.PHONY: all clean

all: server/server client/test_client

server/server: server/server.c protocol/protocol.h
	$(CC) $(CFLAGS) -o server/server server/server.c

client/test_client: client/test_client.c protocol/protocol.h
	$(CC) $(CFLAGS) -o client/test_client client/test_client.c

clean:
	rm -f server/server client/test_client
