//UDP server with reliable data transfer protocol

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PACKETSIZE 1024

void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  int sockfd, portno, clientlen; //client's address size
  struct sockaddr_in serveraddr, clientaddr;
  char buf[PACKETSIZE]; /* message buf */
  int n; /* message byte size */
  struct stat fileStats;

  struct hostent *hostp; /* client host info */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */

  if (argc != 2) {
    fprintf(stderr, "usage: %s < portnumber >\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  //create socket with UDP protocol
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  //Set the server address
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  //bind socket to the port number
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  clientlen = sizeof(clientaddr);
  while (1) {

    //receive a UDP datagram from a client
    bzero(buf, PACKETSIZE);
    n = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    // Check that the file exsits in the directory
    if (stat(buf, &fileStats) == 0) { //file found in the server's wd
     FILE *file = NULL;
     char filedata[PACKETSIZE];
     size_t bytesRead = 0;
     file = fopen(buf, "r");
     if (file != NULL) {
      int packets_sent=0;
      // read up to sizeof(buffer) bytes
      while ((bytesRead = fread(filedata, 1, PACKETSIZE, file)) > 0) {
       n = sendto(sockfd, filedata, strlen(filedata), 0,(struct sockaddr *) &clientaddr, clientlen);
       fprintf(stderr, "packets_sent:%d\n",packets_sent);
       packets_sent++;
       if (n < 0)
        error("ERROR in sendto");
      }
      fclose(file);
     }
    }
    else { //filename not found in the server's wd
      //set the header found bit to 0
      char *notfound = "404 not found";
      n = sendto(sockfd, notfound, strlen(notfound), 0,(struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
       error("ERROR in sendto");
    }
  }
}