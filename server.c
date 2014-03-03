#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#include <ctype.h>

#define HEADER "HTTP/1.1 200 OK \r\n"
#define CONNMAX 1000
#define BYTES 1024
#define ERROR404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 217\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 404 Not Found </title>\r\n</head>\r\n<body>\r\n<p> 404 Not Found </p>\r\n</body>\r\n</html>\r\n"
#define ERROR400 "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 217\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 404 Not Found </title>\r\n</head>\r\n<body>\r\n<p> 404 Not Found </p>\r\n</body>\r\n</html>\r\n"

 //get rid of this
 //get rid of this, no globals!!!!!

void error(char *);
int startServer(char *);
void respond(int, char* ROOT, int clients[]);
void findHost(char *reqline[], int fd);
char* findType(char *reqline);
void response200(int fd, char* reqline[], char path[], char* ROOT);

int main(int argc, char* argv[])
{
    int listenfd, clients[CONNMAX];
    char *ROOT;
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char c;    
  
    //Default Values PATH = ~/ and PORT=10000
    char PORT[6];
    ROOT = getenv("PWD");
    strcpy(PORT,"8001");

    int slot=0;

    //Parsing the command line arguments
    while ((c = getopt (argc, argv, "p:r:")) != -1)
        switch (c)        {
            case 'p':
                strcpy(PORT,optarg);
                break;
            default:
                exit(1);
        }
    
    printf("Started server with port number: %s", PORT);
    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i=0; i<CONNMAX; i++)
        clients[i]=-1;
    listenfd = startServer(PORT);

    // ACCEPT connections
    while (1) //wait for new connections
    {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

        if (clients[slot]<0)
            error ("accept() error"); //there was a problem accepting the connection
        else
        {
            if ( fork()==0 ) //if new process is succesfully spawned
            {
                respond(slot, ROOT, clients); //client number is passed into respond function to deal with requests
                exit(0);
            }
        }

        while (clients[slot]!=-1) slot = (slot+1)%CONNMAX; //incriment the counter for the number of clients
    }

    return 0;
}

//start server
int startServer(char *port) //alot of this is copied from my ns3 lab 1
{
    int listenfd, portint;

	sscanf(port, "%d", &portint);
	
	int type = AF_INET6;

	listenfd = socket(type, SOCK_STREAM, 0);
	if(listenfd == -1) perror("error creting socket");

	struct sockaddr_in6 address;
	
	address.sin6_addr = in6addr_any;

	address.sin6_family = type;
	address.sin6_port = htons(portint);
	if (bind(listenfd, (struct sockaddr *) &address, sizeof(address)) == -1) perror("problem binding");

	if (listen(listenfd, 1000000) == -1) perror("problem listening");

    return listenfd;
    
}

void response400(int fd){

	write(fd, ERROR400, sizeof(ERROR400));
	//close(fd);
	return;
}

void response200(int client, char* reqline[], char path[], char* ROOT){
    int bytes_read, fd;
    strcpy(path, ROOT);
    strcpy(&path[strlen(ROOT)], reqline[1]);
    printf("file requested: %s\r\n", path);

    struct stat fs;

    char data_to_send[BYTES];

    char * copy = malloc(strlen(reqline[1]) + 1); 
    strcpy(copy, reqline[1]);

    char* contentlength = malloc(1024);

    //char* bytesize = malloc(sizeof(int));

    char* type = findType(copy); //find content type
    int counter = 0;
    while (reqline[counter] = strtok (NULL, "\r\n")) counter++;
    findHost(reqline, client); //method checks if host is valid, if it is, it returns to this method and continues, if not, the method calls to send a 400 error

    if ( (fd=open(path, O_RDONLY))!=-1 ){    //FILE FOUND{
        perror("file found");
        fstat(fd, &fs);
        sprintf(contentlength, "Content-Length: %zu\r\n", fs.st_size); //creates string that is the content length header

        write(client, "HTTP/1.0 200 OK\r\n", 17);

        write(client, type, sizeof(type));

        write(client, contentlength, sizeof(contentlength));

        write(client, "Connection: close\r\n\r\n", 21);

        printf("HTTP/1.0 200 OK\r\n");
        printf("Connection: close\r\n");
        printf("%s", type);
        printf("%s", contentlength);

        while ( (bytes_read=read(fd, data_to_send, BYTES))>0 ) write (client, data_to_send, bytes_read);
        free(copy);
        free(contentlength);
    }

    else write(client, ERROR404, strlen(ERROR404)); //FILE NOT FOUND, 404 ERROR

}

//client connection
void respond(int n, char* ROOT, int clients[]){
	char mesg[99999], *reqline[128],  path[99999];
	int rcvd;

    memset( (void*)mesg, (int)'\0', 99999 );

    rcvd=recv(clients[n], mesg, 99999, 0);

    if (rcvd<0)    // receive error
       	fprintf(stderr,("recv() error\n")); //no request
    else if (rcvd==0)    // receive socket closed
       	fprintf(stderr,"Client disconnected upexpectedly.\n");
    else{    // message received{
        printf("%s", mesg);
        reqline[0] = strtok (mesg, " \t\n"); //part of the request that should be "GET"

        if ( strncmp(reqline[0], "GET\0", 4)!=0 ){
        	printf("\n\nreqline0 %s", reqline[0]);
        	response400(clients[n]);
        }

        else if ( strncmp(reqline[0], "GET\0", 4)==0 ){
            reqline[1] = strtok (NULL, " \t"); //the file being requested

            reqline[2] = strtok (NULL, " \t\n"); //the part of the request that should say "HTTP/1.1" or "HTTP/1.0"
            if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 ){
            	printf("\n\nreqline2 %s", reqline[2]);
				response400(clients[n]);
            }
            else{
            	if ( strncmp(reqline[1], "/\0", 2)==0 ) reqline[1] = "/index.html"; //because if no file is specified, index.html will be opened by default
            	response200(clients[n], reqline, path, ROOT);                
            	}
        	}
    	}

    //Closing SOCKET
    shutdown (clients[n], SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close(clients[n]);
    clients[n]=-1;
}

void findHost(char *reqline[], int fd){ //identifies the host header in the request, this is needed since im not assuming its in a static position
	int c = 0;
	char* h;
	char* host;
	char* currenthost = malloc(1024);
	
	char* domain = ".dcs.gla.ac.uk";
	char* local = "localhost";
		
	gethostname(currenthost, 1024);

	char* hostdomain;

	char* p = currenthost;

	for ( ; *p; ++p) *p = tolower(*p);

	while(c < 50){ //50 is arbitary, but there wont be more than 50 headers in a request so its an easy solution to a nonproblem

	if ( strncmp(reqline[c], "Host: ", 6)==0 ){ //finds the host header

		h = strdup(reqline[c]+6);	
		host = strtok(h,":");
		hostdomain = strdup(host);
		strncat(hostdomain, domain, 64);

		if (strcmp(currenthost, local) != 0 && strcmp(currenthost, host) != 0 && strcmp(currenthost, hostdomain) !=0 && strcmp(host, "localhost") != 0) response400(fd); //need to remove last strcmp, figure out why it wont send http request when using the computer name
		c = 50; //makes it exit the loop
        free(h);
        //free(host);
        free(currenthost);
        free(hostdomain);
		}
	else c++;
	}
}

char* findType(char *reqline){ //identifies the host header in the request, this is needed since im not assuming its in a static position
	char * point;
	char* extension = strtok_r(reqline+1, ".", &point); //this was previously strtok, but having multiple calls to different strtoks was not working, this fixes the issue
	extension = strtok_r(NULL, ".", &point);

	char * content_Type;

	/* Get the correct "Content-Type: " message */
	if(!strncmp(extension, "html", 4)){
		content_Type = "Content-Type: text/html\r\n";
	}else if(!strncmp(extension, "htm", 3)){
		content_Type = "Content-Type: text/html\r\n";
	}else if(!strncmp(extension, "txt", 3)){
		content_Type = "Content-Type: text/plain\r\n";
	}else if(!strncmp(extension, "jpg", 3)){
		content_Type = "Content-Type: image/jpeg\r\n";
	}else if(!strncmp(extension, "jpeg", 4)){
		content_Type = "Content-Type: image/jpeg\r\n";
	}else if(!strncmp(extension, "gif", 3)){
		content_Type = "Content-Type: image/gif\r\n";
	}else{
		content_Type = "Content-Type: application/octet-stream\r\n";
	}

	//printf("%s\n\n\n", content_Type);
	return content_Type;
}