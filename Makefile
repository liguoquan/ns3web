CFLAGS=-W -Wall -g -lpthread

all: server clean

server: server.o
	gcc $(CFLAGS) -o server server.o

server.o: server.c
	gcc $(CFLAGS) -c server.c

clean:
	rm -rf *.o 
