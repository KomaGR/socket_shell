.PHONY: all clean
CC=gcc
CCFLAGS=-Wall

all: server client

server: server.o
	${CC} ${CCFLAGS} server.c -o server

client: client.o
	${CC} ${CCFLAGS} client.c -o client

clean:
	rm -f server client
