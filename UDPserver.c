//UDP server with reliable data transfer protocol

struct TCP_PACKET {
  int seq_num;
  int ack_num;    
  int window_size;
  int ACK_flag;
  int SYN_flag;
  int FIN_flag;
  int FILEFOUND_flag;
  char data[996];
};

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
  //Receive the SYN segment
  struct TCP_PACKET synSegment;
  n = recvfrom(sockfd, &synSegment, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
  if (n < 0)
    error("ERROR in recvfrom");
  fprintf(stderr, "SYN segment received:\nSeq number:%d\nSYN flag:%d\nACK flag:%d\n",synSegment.seq_num,synSegment.SYN_flag,synSegment.ack_num);

  //SYNACK segment
  struct TCP_PACKET synackSegment;
  synackSegment.seq_num = 42;
  synackSegment.ack_num = synSegment.seq_num+1;
  synackSegment.window_size = 5120;
  synackSegment.ACK_flag = 0;
  synackSegment.SYN_flag = 1;
  synackSegment.FIN_flag = 0;
  synackSegment.FILEFOUND_flag = 0;
  bzero(synackSegment.data,996);

  //send the SYNACK Segment
  n = sendto(sockfd, &synackSegment, sizeof(synackSegment), 0, &clientaddr, clientlen);
  if (n < 0) 
    error("ERROR in sendto for synSegment");

  //Receive the last SYN segment
  struct TCP_PACKET lastsynSegment;
  n = recvfrom(sockfd, &lastsynSegment, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
  if (n < 0)
    error("ERROR in recvfrom");
  fprintf(stderr, "SYN segment received:\nSeq number:%d\nSYN flag:%d\nACK flag:%d\nData:%s\n",lastsynSegment.seq_num,lastsynSegment.SYN_flag,lastsynSegment.ack_num,lastsynSegment.data);

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
     bzero(filedata, PACKETSIZE);
     size_t bytesRead = 0;
     file = fopen(buf, "r");
     if (file != NULL) {
      int packets_sent=0;
      // read up to sizeof(buffer) bytes
      while ((bytesRead = fread(filedata, 1, PACKETSIZE, file)) > 0) {
       n = sendto(sockfd, filedata, strlen(filedata), 0,(struct sockaddr *) &clientaddr, clientlen);
       bzero(filedata, PACKETSIZE);
       fprintf(stderr, "packets #%d sent, with %d bytes\n",packets_sent,n);
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