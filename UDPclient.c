//UDP client with reliable data transfer protocol

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

  //Initiate establishing a connection, SYN Segment
  struct TCP_PACKET synSegment;
  synSegment.seq_num = 10;
  synSegment.ack_num = 0;
  synSegment.window_size = 5120;
  synSegment.ACK_flag = 0;
  synSegment.SYN_flag = 1;
  synSegment.FIN_flag = 0;
  synSegment.FILEFOUND_flag = 0;
  bzero(synSegment.data,996);

  //send the SYN Segment
  serverlen = sizeof(serveraddr);
  n = sendto(sockfd, &synSegment, sizeof(synSegment), 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto for synSegment");

  //receive SYNACK from the server
  struct TCP_PACKET synackSegment;
  n = recvfrom(sockfd, &synackSegment, PACKETSIZE, 0, &serveraddr, &serverlen);
  if (n < 0) 
    error("ERROR in recvfrom");
  fprintf(stderr, "SYNACK segment received:\nSeq number:%d\nSYN flag:%d\nACK num:%d\n",synackSegment.seq_num,synackSegment.SYN_flag,synackSegment.ack_num);

  //send the final SYN Segment
  struct TCP_PACKET lastsynSegment;
  lastsynSegment.seq_num = 11;
  lastsynSegment.ack_num = synackSegment.seq_num+1;
  lastsynSegment.window_size = 5120;
  lastsynSegment.ACK_flag = 0;
  lastsynSegment.SYN_flag = 0;
  lastsynSegment.FIN_flag = 0;
  lastsynSegment.FILEFOUND_flag = 0;
  bzero(lastsynSegment.data,996);
  strcpy(lastsynSegment.data, filename); // copy dir name   

  //send the final SYN Segment with the filename
  n = sendto(sockfd, &lastsynSegment, sizeof(lastsynSegment), 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto for synSegment");
  
//  // send the message(filename) to the server
//  n = sendto(sockfd, filename, strlen(filename), 0, &serveraddr, serverlen);
//  if (n < 0) 
//    error("ERROR in sendto");
  
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