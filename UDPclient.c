//UDP client with reliable data transfer protocol

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define PACKETSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n, serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char *filename;
    char buf[PACKETSIZE];

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"Usage: %s < server_hostname > < server_portnumber > < filename >\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];
    fprintf(stderr, "filename is:%s\n",filename); //debugging

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    bzero(buf, PACKETSIZE);

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, filename, strlen(filename), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    n = recvfrom(sockfd, buf, PACKETSIZE, 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    
    //check the found flag
    
    //if file found
    char *dir = "received.data/";
    char* file_path = malloc(strlen(filename)+15); /* make space for the new string (should check the return value ...) */
    strcpy(file_path, dir); /* copy name into the new var */
    strcat(file_path, filename); /* add the extension */
    /* Store the server's reply in the file*/
    int fd = open(file_path,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR);
    int fin = 1;
    int num_packets=2;
    while(fin) {
     write(fd,buf,n);
     n = recvfrom(sockfd, buf, PACKETSIZE, 0, &serveraddr, &serverlen);
     fprintf(stderr, "%d\n",n);
     num_packets--;
     if (num_packets==0) {
      fin = 0;
     } 
     if (n < 0) 
       error("ERROR in recvfrom");
    }
    write(fd,buf,n);

    //else (file not found)
    //display 404 not found
    //end the connection
    return 0;
}