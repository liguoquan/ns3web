 all: server.c
	gcc -g -lpthread -Wall -o server server.c