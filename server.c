#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<signal.h>
#include<fcntl.h>

#define HEADER "HTTP/1.1 200 OK \r\n"
#define CONNMAX 1000
#define BYTES 1024
#define ERROR404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 217\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 404 Not Found </title>\r\n</head>\r\n<body>\r\n<p> 404 Not Found </p>\r\n</body>\r\n</html>\r\n"
#define ERROR400 "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 217\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 404 Not Found </title>\r\n</head>\r\n<body>\r\n<p> 404 Not Found </p>\r\n</body>\r\n</html>\r\n"

char *ROOT; //get rid of this
int listenfd, clients[CONNMAX]; //get rid of this, no globals!!!!!
void error(char *);
void startServer(char *);
void respond(int);
void findHost(char *reqline[], int fd);
char* findType(char *reqline);
void response200(int fd, char* reqline[], char path[], char data_to_send[]);

int main(int argc, char* argv[])
{
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char c;    
    
    //Default Values PATH = ~/ and PORT=10000
    char PORT[6];
    ROOT = getenv("PWD");
    strcpy(PORT,"10000");

    int slot=0;

    //Parsing the command line arguments
    while ((c = getopt (argc, argv, "p:r:")) != -1)
        switch (c)
        {
            case 'r':
                ROOT = malloc(strlen(optarg));
                strcpy(ROOT,optarg);
                break;
            case 'p':
                strcpy(PORT,optarg);
                break;
            case '?':
                fprintf(stderr,"Wrong arguments given!!!\n");
                exit(1);
            default:
                exit(1);
        }
    
    printf("Server started at port no. %s%s%s with root directory as %s%s%s\n","\033[92m",PORT,"\033[0m","\033[92m",ROOT,"\033[0m");
    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i=0; i<CONNMAX; i++)
        clients[i]=-1;
    startServer(PORT);

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
                respond(slot); //client number is passed into respond function to deal with requests
                exit(0);
            }
        }

        while (clients[slot]!=-1) slot = (slot+1)%CONNMAX; //incriment the counter for the number of clients
    }

    return 0;
}

//start server
void startServer(char *port) //alot of this is copied from my ns3 lab 1
{

	int portint;
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

    
}

void response400(int fd){
	//int error_Check;
	//error_Check = write(fd, "HTTP/1.0 400 Bad Request\n", 25);
	//if(error_Check<0){
	//	fprintf(stderr, "Error writing response: 400\n");
	//}
	//perror("4000000000000000000 error\n\n\n");
	write(fd, ERROR400, sizeof(ERROR400));
	//close(fd);
	return;
}

void response200(int client, char* reqline[], char path[], char data_to_send[]){
    int bytes_read, fd;
    strcpy(path, ROOT);
    strcpy(&path[strlen(ROOT)], reqline[1]);
    printf("file: %s\n", path);

    struct stat fs;

    char * copy = malloc(strlen(reqline[1]) + 1); 
	strcpy(copy, reqline[1]);

	char* contentlength = malloc(1024);
	//contentlength = "Content-length: ";

	char* bytesize = malloc(sizeof(int));



    char* type = findType(copy);
	int counter = 0;
	while (reqline[counter] = strtok (NULL, "\r\n")) counter++;
	findHost(reqline, client);
//printf("%i\n", temp);

    if ( (fd=open(path, O_RDONLY))!=-1 ){    //FILE FOUND{
		perror("file found");
		fstat(fd, &fs);
		perror("fstat ok");
		//sprintf(bytesize, "%d", fs.st_size);
		sprintf(contentlength, "Content-Length: %d\r\n\r\n", fs.st_size);
		//perror("sprint ok");
		//printf("\n%s BYTE SIZE\n", bytesize);
		//strncat(contentlength, bytesize, 32);
		//perror("concat ok");
		//printf("\n\n\nTHIS IS THE CONTENTLENGTH\n%s", contentlength);

        send(client, "HTTP/1.0 200 OK\r\n", 17, 0);
        printf("HTTP/1.0 200 OK\r\n");
        send(client, "Connection: close\r\n", 19, 0);
        printf("Connection: close\r\n");
        send(client, type, sizeof(type), 0);
        printf("%s", type);
        send(client, contentlength, sizeof(contentlength), 0);
        printf("%s", contentlength);
        while ( (bytes_read=read(fd, data_to_send, BYTES))>0 ){
        	//perror("lololol");
           	write (client, data_to_send, bytes_read);}
    }
    else{
		write(client, ERROR404, strlen(ERROR404)); //FILE NOT FOUND, 404 ERROR
	}
}

//client connection
void respond(int n){
	char mesg[99999], *reqline[128], data_to_send[BYTES], path[99999];
	int rcvd, fd, bytes_read;

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
            //char* p = strdup(reqline[1]);
           
            //findType(copy);
           // printf("\n\n\n\n%s\n\n\n\n", copy);
			////////////////findType(reqline[1]);
            reqline[2] = strtok (NULL, " \t\n"); //the part of the request that should say "HTTP/1.1" or "HTTP/1.0"
            if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 ){
                	//write(clients[n], "HTTP/1.0 400 Bad Request\n", 25); //the request sent was not a valid HTTP request
            	printf("\n\nreqline2 %s", reqline[2]);
				response400(clients[n]);
            }
            else{
            	if ( strncmp(reqline[1], "/\0", 2)==0 ) reqline[1] = "/index.html"; //because if no file is specified, index.html will be opened by default
            	response200(clients[n], reqline, path, data_to_send);                
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
	//perror("here lol");
	
		
	gethostname(currenthost, 1024);
	//perror("gethostname");
	char* hostdomain;
	//perror("here lol1");
	//printf("%s current host\n", currenthost);
	char* p = currenthost;
	//printf("%s    copied name", currenthost);
	// Conversion to lower case.
	// Taken from stack overflow: J.F. Sebastian
	// https://stackoverflow.com/a/2661788
	for ( ; *p; ++p) *p = tolower(*p);

	//printf("%s current host\n", currenthost);
	//strncat(hostdomain, domain, 64);
	//printf("%sCURRENTHOSTLOL\n", );
	//printf("%i\n", sizeof(reqline) / sizeof(reqline[]));
	while(c < 50){ //50 is arbitary, but there wont be more than 50 headers in a request so its an easy solution to a nonproblem
	//printf("%i\n", c);
	if ( strncmp(reqline[c], "Host: ", 6)==0 ){
		//printf("%s\n", reqline[c]);	
		//h = strcpy(h, reqline[c]+6);
		h = strdup(reqline[c]+6);	
		//printf("%s\n", h);
		host = strtok(h,":");
		//printf("%s\n", host);

		hostdomain = strdup(host);
		strncat(hostdomain, domain, 64);
		//printf("%s    %s    %s      %s\n", local, host, hostdomain, currenthost);
		//printf("%i   %i   %i\n", strcmp(currenthost, local), strcmp(currenthost, host), strcmp(currenthost, hostdomain));
		if (strcmp(currenthost, local) != 0 && strcmp(currenthost, host) != 0 && strcmp(currenthost, hostdomain) !=0 && strcmp(host, "localhost") != 0) response400(fd); //need to remove last strcmp, figure out why it wont send http request when using the computer name
		c = 50;
		}
	else c++;
	}
//perror("out of here");	

}

char* findType(char *reqline){ //identifies the host header in the request, this is needed since im not assuming its in a static position
	//char* p;
	//p = strdup(reqline);
	char * point;
	char* extension = strtok_r(reqline+1, ".", &point);
	extension = strtok_r(NULL, ".", &point);

	char * content_Type;
	char * response;

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



