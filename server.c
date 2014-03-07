#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BUFFERSIZE 512
#define ERROR404 "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 202\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 404 File Not Found </title>\r\n</head>\r\n<body>\r\n<p> 404 Not Found </p>\r\n</body>\r\n</html>\r\n"
#define ERROR400 "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 201\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 400 Bad Request </title>\r\n</head>\r\n<body>\r\n<p> 400 Bad Request </p>\r\n</body>\r\n</html>\r\n"
#define ERROR500 "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 221\r\nConnection: close\r\n\r\n<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\"><html>\r\n<head>\r\n<title> 500 Internal Server Error </title>\r\n</head>\r\n<body>\r\n<p> 500 Internal Server Error </p>\r\n</body>\r\n</html>\r\n"

void error(char *);
void findHost(char *requestLine[], int fd);
char* findType(char *requestLine);
void response(int fd, char* requestLine[], char path[], char* root);
void* pthread_run(void* args);
void response200(int client, char* contentLength, char* copy, int filefd);
void response400(int fd);
void response404(int fd);
void response500(int fd);

void* pthread_run(void* args) {
    char* root = getenv("PWD");
    int client = *(int*)args;

    char request[65536], *requestLine[512],  path[4096];
    memset((void*)request, '\0', 65536);

    int rcvd;
    rcvd = recv(client, request, 65536, 0); //reads in entire request to a huge buffer, it wont overflow

    if (rcvd < 0){
        printf(("request is empty\n")); //no request
        response400(client);
    }    // receive error
        
    else if (rcvd == 0) printf("client disconnected unexpectedly\n"); // receive socket closed
    else{    // message received
        printf("%s", request);
        requestLine[0] = strtok (request, " \r\n"); //part of the request that should be "GET"

        if ( strncmp(requestLine[0], "GET\0", 4) == 0 ){
            requestLine[1] = strtok (NULL, " \r"); //the file being requested
            requestLine[2] = strtok (NULL, " \r\n"); //the part of the request that should say "HTTP/1.1"
            
            if (strncmp( requestLine[2], "HTTP/1.1", 8) != 0){
                response400(client);
            }

            else{
                if (strncmp(requestLine[1], "/\0", 2) == 0) 
                    requestLine[1] = "/index.html"; //if no file is specified then index.html will be opened by default
                    response(client, requestLine, path, root); // go into response                
                }
            }

        else if ( strncmp(requestLine[0], "GET\0", 4) !=0 ){
            response400(client);
        }
    }

    close(client); //close the socket
    return 0;
}

int main(int argc, char* argv[])
{
    int listenfd;
    char *root;
    struct sockaddr_in clientaddr;
    socklen_t addrlen;
    char argument;    
  
    //default port is 8080 but can be changed with argument "-p PORTNO"
    char port[6];

    root = getenv("PWD");
    strcpy(port,"8080");

    int slot=0;

    //get args
    while ((argument = getopt (argc, argv, "p:")) != -1)
        switch (argument){
            case 'p':
                strcpy(port,optarg);
                break;
            default:
                exit(1);
        }
    
    fprintf(stdout, "Started the server with port number: %s", port);

    int portint;
    sscanf(port, "%d", &portint);
    
    int type = AF_INET6;

    listenfd = socket(type, SOCK_STREAM, 0);

    if(listenfd == -1) perror("error creting socket");

    struct sockaddr_in6 address;
    
    address.sin6_addr = in6addr_any;
    address.sin6_family = type;
    address.sin6_port = htons(portint);

    if (bind(listenfd, (struct sockaddr *) &address, sizeof(address)) == -1){ //bind the socket to a port
        perror("problem binding");
        close(listenfd);
        return 0;
    } 

    if (listen(listenfd, 65536) == -1){
        perror("problem listening");
        close(listenfd);
        return 0;
    } 

    //accept connections
    while (1){ //wait for new connections
        addrlen = sizeof(clientaddr);
        slot = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

        if (slot <0) error ("accept() error"); //there was a problem accepting the connection
        else{
            pthread_t thread;
            pthread_create(&thread, NULL, pthread_run, (void*)&slot);           
        }
       
    }
    close(listenfd);
    return 0;
}

void response(int client, char* requestLine[], char path[], char* root){
    
    int filefd;
    strcpy(path, root);
    strcpy(&path[strlen(root)], requestLine[1]);

    printf("File requested: %s\r\n", path);

    char * copy = calloc(1024,strlen(requestLine[1]) + 1); 
    if (copy == NULL){
        perror("CALLOC FAILED COPY IN RESPONSE200\n");
        response500(client);
        close(client);
        free(copy);
    } 
    strcpy(copy, requestLine[1]); //holds a copy of the file request

    char* contentLength = calloc(1024,1024);
    if (contentLength == NULL){
        perror("CALLOC FAILED CONTENTLENGTH IN RESPONSE200\n");
        response500(client);
        close(client);
        free(contentLength);
        free(copy);
    } 

    int counter = 0;
    while ( (requestLine[counter] = strtok (NULL, "\r\n")) ) counter++;
    findHost(requestLine, client); //method checks if host is valid, if it is, it returns to this method and continues, if not, the method calls to send a 400 error

    if ( (filefd=open(path, O_RDONLY))!=-1 ){ //file is found
        response200(client, contentLength, copy, filefd);
    }
    else{
        response404(client); //otherwise file not found so send 404 error
        free(copy);
        free(contentLength);
    } 
}

void response404(int fd){
    write(fd, ERROR404, sizeof(ERROR404));
    close(fd);
}

void response500(int fd){
    write(fd, ERROR500, sizeof(ERROR500));
    close(fd);
}

void response400(int fd){
	write(fd, ERROR400, sizeof(ERROR400));
	close(fd);
}

void response200(int client, char* contentLength, char* copy, int filefd){
    struct stat fs;
    int bytes_read;
    char dataToSend[BUFFERSIZE]; //buffer to hold data being read in from file

    printf("file found");

    char* type = findType(copy); //find content type

    fstat(filefd, &fs);
    sprintf(contentLength, "Content-Length: %zu\r\n", fs.st_size); //creates string that is the content length header

    //below I send (write) all the headers for the request. There is NO need to concat everything into one huge string, so I dont
    write(client, "HTTP/1.1 200 OK\r\n", 17);
    write(client, type, sizeof(type)); //content type
    write(client, contentLength, sizeof(contentLength)); //content length
    write(client, "Connection: close\r\n\r\n", 21); //connection close

    while ( (bytes_read=read(filefd, dataToSend, BUFFERSIZE)) > 0 ) write (client, dataToSend, bytes_read); //reading and writing the file in chunks of size BUFFERSIZE

    free(copy);
    free(contentLength);
    close(filefd);
    close(client);

}

void findHost(char *requestLine[], int fd){ //identifies the host header in the request, this is needed since im not assuming its in a static position
	int c = 0;
	char* h;
	char* requesthost;

	char* currenthost = calloc(1024, 1024);
	if (currenthost == NULL){
        perror("CALLOC FAILED IN FINDHOST\n");
        response500(fd);
    } 
			
	gethostname(currenthost, 1024);

    //this while loop just loops through the headers to find the host header
    //50 is arbitary, but there wont be more than 50 headers in a request so its an easy solution to a nonproblem
	while(c < 50){ 

    	if ( strncmp(requestLine[c], "Host: ", 6)==0 ){ //finds the host header

    		h = strdup(requestLine[c]+6); //gets the host from the request	
    		requesthost = strtok(h,":"); //split it by the semi colon to remove the port

    		if (strcmp(requesthost, currenthost) != 0 && strcmp(requesthost, "localhost") != 0){
                response400(fd);
                close(fd);
            } 

    		c = 50; //makes it exit the loop because host was found
            free(currenthost);
            free(requesthost);
    	}
        else if(c == 50){ //if this happens then the request was too big (Over 50 headers)
            response500(fd);
            free(currenthost);
            free(requesthost);
        }
    	else c++;
	}
}

char* findType(char *requestLine){ //identifies the host header in the request, this is needed since im not assuming its in a static position
	char * point;
	char* extension = strtok_r(requestLine+1, ".", &point); //this was previously strtok, but having multiple calls to different strtoks was not working, this fixes the issue

    extension = strtok_r(NULL, ".", &point);

	char * contentType;

	//get the correct content-type
    if(extension == NULL){
        contentType = "Content-Type: application/octet-stream\r\n"; //need to do this check first otherwise it caueses problems, allows files without an extension
	}
    else if(!strncmp(extension, "html", 4) || !strncmp(extension, "htm", 3)){
		contentType = "Content-Type: text/html\r\n";
	}
    else if(!strncmp(extension, "txt", 3)){
		contentType = "Content-Type: text/plain\r\n";
	}
    else if(!strncmp(extension, "jpg", 3) || !strncmp(extension, "jpeg", 4)){
		contentType = "Content-Type: image/jpeg\r\n";
	}
    else if(!strncmp(extension, "gif", 3)){
		contentType = "Content-Type: image/gif\r\n";
	}
    else{
		contentType = "Content-Type: application/octet-stream\r\n";
	}

	return contentType;
}