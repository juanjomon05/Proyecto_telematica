CC = gcc
CFLAGS = -Wall -pthread

.PHONY: all clean

all: server/server client/test_client client/pong_client

server/server: server/server.c server/logger.h server/game.h protocol/protocol.h
	$(CC) $(CFLAGS) -o server/server server/server.c

client/test_client: client/test_client.c protocol/protocol.h
	$(CC) $(CFLAGS) -o client/test_client client/test_client.c

client/pong_client: client/pong_client.c protocol/protocol.h
	$(CC) $(CFLAGS) -o client/pong_client client/pong_client.c

clean:
	rm -f server/server client/test_client client/pong_client
