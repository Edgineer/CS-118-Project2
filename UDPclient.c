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
  char *filename;
  char buf[PACKETSIZE];

  /* check command line arguments */
  if (argc != 4) {
    fprintf(stderr,"Usage: %s < server_hostname > < server_portnumber > < filename >\n", argv[0]);
    exit(0);
  }

  portno = atoi(argv[2]);
  //add portnumber range check
  filename = argv[3];
  
  // socket: create the socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr,"ERROR, host %s not found\n", argv[1]);
    exit(0);
  }

  // build the server's Internet address 
  bzero((char *) &serveraddr, sizeof(serveraddr)); //clear server address information
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
	(char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  bzero(buf, PACKETSIZE);//clear receiving buffer

  // send the message(filename) to the server
  serverlen = sizeof(serveraddr);
  n = sendto(sockfd, filename, strlen(filename), 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto");
  
  //receive server response into buf
  n = recvfrom(sockfd, buf, PACKETSIZE, 0, &serveraddr, &serverlen);
  if (n < 0) 
    error("ERROR in recvfrom");
    
  //check the found flag
    
  //if file found
  char *dir = "received.data/";
  char* file_path = malloc(strlen(filename)+15); // allocaate for complete filepath including directory (should check the return value)
  strcpy(file_path, dir); // copy dir name 
  strcat(file_path, filename); // add filename 
  /* Store the server's reply in the file*/
  int fd = open(file_path,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR);
  int fin = 1;
  int packets_received=0;
  while(fin) {
    write(fd,buf,n);
    fprintf(stderr, "wrote %d bytes to file on packet #%d\n",n,packets_received);
    bzero(buf, PACKETSIZE);
    n = recvfrom(sockfd, buf, PACKETSIZE, 0, &serveraddr, &serverlen);
    packets_received++;
    if (n < 0) 
      error("ERROR in recvfrom");
  }

    //else (file not found)
    //display 404 not found
    //end the connection
    return 0;
}