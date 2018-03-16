//server.c selective repeat file transmission
struct TCP_PACKET {
  int seq_num;
  int ack_num;    
  int window_size;
  int SYN_flag;
  int FIN_flag;
  int FILENOTFOUND_flag;
  char data[1000];
};

struct packet_check {
	int isACKed;
	int seqNum;
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
#include <signal.h>
#include <poll.h>
#include <time.h>

#define HEADERSIZE 24
#define PACKETSIZE 1024
#define DATASIZE 1000
#define MAX_SEQUENCE_NUMBER 30720
#define WINDOW_SIZE 5120
#define RTO 500//ms

int numTotalPackets = 1;
int curWindow  = 0;
int send_base = 42;
int nextseqnum = 42;
int curACKedPackets = 0;
int numSentUnacked = 0;

//The server knows how many packets it needs to send and the bytes so can calculate final sequence number
//send base and nextseqnum
//establish connection and get the filename, the server now begins to send the packets with window size = 5,

//create an structure to keep track of the packets that have been received (has packet number and has packet been received?)

//Use threads to send the packets and time them, when the packet is arrived exit the thread function

//on the poll acknowledge the structure

//on another checking loop go through the packets structure loop if the send_base = the packet's sequence number and it has been ACK'd
//then increase the send_base and also increase the number of packets sent or is all packets ACK'd then go to close connection mode

void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  int sockfd, portno, clientlen; //client's address size
  struct sockaddr_in serveraddr, clientaddr;
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
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

  //Set the server address
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  //bind socket to the port number
  if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) 
    error("ERROR on binding");
  
  clientlen = sizeof(clientaddr);
 
  while (1) {
  	numTotalPackets = 1;
	curWindow  = 0;
	send_base = 42;
	nextseqnum = 42; //you sent out the SYNACK so nextseqnum by 1
	curACKedPackets = 0;

  /**
   *
   *  Handshake
   * 
   *  Assume SYN segment has been received and SYNACK segment has been sent
   *  Next_seqnum = server initial sequence number (42)
   *  Receive the last SYN segment containing the filename
   *
   */
	nextseqnum = 43;

	struct TCP_PACKET lastsynSegment;
	n = recvfrom(sockfd, &lastsynSegment, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
	if (n < 0)
	  error("ERROR in recvfrom");
	if (lastsynSegment.ack_num == send_base+1){//send_base+data in the packet
	  send_base = lastsynSegment.ack_num;
	}
	else
	  error("Invalid last synack"); //cant proceed until I recieve the lastsynSegment
	fprintf(stderr, "Receiving packet %d \n",lastsynSegment.ack_num);
	
	/*
	*
	*	End of Connection Handshake
	*
	*/
	//gauranteed to receive the client's request
	if (stat(lastsynSegment.data, &fileStats) == 0) { //file found in the server's wd
	  FILE *file = NULL;
	  char filedata[DATASIZE];
      bzero(filedata, DATASIZE);
      size_t bytesRead = 0;
	  numTotalPackets = (long long) fileStats.st_size/(DATASIZE-1); //get the total number of packets needed for data transmission
	  if ((fileStats.st_size%(DATASIZE-1)!=0)) {numTotalPackets++;}
	  fprintf(stderr, "Total number of packets = %d\n",numTotalPackets);
	  file = fopen(lastsynSegment.data, "r");
	  if (file == NULL)
	    error("opening file to read");

	  struct pollfd fdArr[1];
      fdArr[0].fd = sockfd;
      fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR	  
	  
	  int numTimes=0;
	  while(curACKedPackets < numTotalPackets){
	  	numTimes++;
	  	if (numSentUnacked < 5) {
	      struct TCP_PACKET dataPacket;
	      dataPacket.seq_num = nextseqnum;
	      dataPacket.ack_num = lastsynSegment.seq_num + 1;
	      dataPacket.window_size = WINDOW_SIZE;
	      bzero(dataPacket.data,DATASIZE);
	      
	      bytesRead = fread(filedata, 1, DATASIZE-1, file);
	      if (bytesRead<0) {error("ERROR reading from file");}
	      snprintf(dataPacket.data, bytesRead+1, "%s", filedata);
	   	  bzero(filedata, DATASIZE);
	      
	      n = sendto(sockfd, &dataPacket, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
	      if (n < 0) 
	        error("ERROR in sendto");
	      fprintf(stderr, "Sending packet %d %d %s\n",dataPacket.seq_num,dataPacket.window_size);
	      nextseqnum += (HEADERSIZE + strlen(dataPacket.data));
	  	  numSentUnacked++;
	  	}
	  	  
	  	//use poll to wait for a packet, read the packet and create an ACK for it   
        poll(fdArr,1,500);//500 is the length of the timeout
      
        if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
          struct TCP_PACKET ACK;
		  n = recvfrom(sockfd, &ACK, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
		  if (n < 0)
	  	    error("ERROR in recvfrom ACK");
	  	  if (ACK.ack_num == nextseqnum) {
	  	  	send_base = nextseqnum;
	  	  	curACKedPackets+=1;
	  	  	fprintf(stderr, "Receiving packet %d\n",ACK.ack_num);
	  	  	numSentUnacked--;
	  	  }
	  	  else {
	  	    fprintf(stderr, "Receiving packet %d (out of order)\n",ACK.ack_num);
	  	  }
	    }
	  }
	  fprintf(stderr, "Entered loop %d times\n",numTimes);
	}

	else { //filename not found in the server, there is only one packet which needs to be sent and ACKed
	  
	  //Create the 404 Packet
	  struct TCP_PACKET seg404;
	  seg404.seq_num = nextseqnum;
	  seg404.ack_num = lastsynSegment.seq_num + 1;
	  seg404.FILENOTFOUND_flag = 1;
	  seg404.window_size = WINDOW_SIZE;
	  bzero(seg404.data,DATASIZE);
	  snprintf(seg404.data,14, "%s","404 NOT FOUND");
	  
	  //Create the packet check object
	  struct packet_check pack404;
	  pack404.isACKed=0;
	  pack404.seqNum = nextseqnum + HEADERSIZE + strlen(seg404.data);
	
	  n = sendto(sockfd, &seg404, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
	  if (n < 0)
	    error("ERROR in sendto");
	  time_t start_t;
	  time(&start_t);
	  fprintf(stderr, "Sending packet %d %d\n",seg404.seq_num,seg404.window_size);
	  nextseqnum = nextseqnum + HEADERSIZE + strlen(seg404.data);
	  
	  struct pollfd fdArr[1];
      fdArr[0].fd = sockfd;
      fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR	  
	  while(pack404.isACKed == 0) {
	    time_t end_t;
	  	time(&end_t);
	    if (difftime(end_t,start_t) > 0.5) { //retransmit the packet
	      n = sendto(sockfd, &seg404, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
	      if (n < 0)
	        error("ERROR in sendto");
	      fprintf(stderr, "Sending packet %d %d Retransmission\n",seg404.seq_num,seg404.window_size);
	      time(&start_t);	
	    }

	    //use poll to wait for a packet, Extract packet data
        poll(fdArr,1,0);//0 is the length of the timeout
        if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
          struct TCP_PACKET ACK;
		  n = recvfrom(sockfd, &ACK, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
		  if (n < 0)
	  	    error("ERROR in recvfrom ACK");
	  	  if (ACK.ack_num == pack404.seqNum) {
	  	  	pack404.isACKed = 1;
	  	  	send_base = ACK.ack_num;
	  	  	fprintf(stderr, "Receiving packet %d\n",ACK.ack_num);
	  	  }
	    }
	  }
	}

	/*
	*
	*
	*	Close Connection Protocol
	*
	*
	*/
  }
}