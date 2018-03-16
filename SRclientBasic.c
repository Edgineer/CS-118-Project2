//client.c selective repeat file transmission
struct TCP_PACKET {
  int seq_num;
  int ack_num;    
  int window_size;
  int SYN_flag;
  int FIN_flag;
  int FILENOTFOUND_flag;
  char data[1000];
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
#include <signal.h>
#include <poll.h>

#define HEADERSIZE 24
#define PACKETSIZE 1024
#define WINDOW_SIZE 5120
#define DATA_SIZE 1000

int client_seqnum = 10;
int rcv_base = 0; //should be equal to the server initial sequence number
int fileFound = 1;
int fd = 0;
char *dir = "received.data/";
char* file_path;

void error(char *msg) {
  perror(msg);
  exit(0);
}

int main(int argc, char *argv[]) {
  int sockfd, portno, n, serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *filename;

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
  serverlen = sizeof(serveraddr);

  //send the final SYN Segment
  struct TCP_PACKET lastsynSegment;
  lastsynSegment.seq_num = client_seqnum+1; //SYN segment has initial client_seqnum
  lastsynSegment.ack_num = 43; //should be the SYNACK packet sequence number + 1
  lastsynSegment.window_size = WINDOW_SIZE;
  bzero(lastsynSegment.data,DATA_SIZE);
  snprintf(lastsynSegment.data,strlen(filename)+1, "%s",filename);//copy dir name +1 for null byte

  rcv_base = 43;

  //send the final SYN Segment with the filename
  n = sendto(sockfd, &lastsynSegment, PACKETSIZE, 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto for lastsynSegment");
  client_seqnum++;
  fprintf(stderr,"Sending packet %d\n",lastsynSegment.ack_num);

  struct pollfd fdArr[1];
  fdArr[0].fd = sockfd;
  fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR
  
  int fileDetermined = 0;
  struct TCP_PACKET packet1;

  while(fileDetermined==0) {
    //use poll to wait for a packet, read the packet and create an ACK for it   
    poll(fdArr,1,0);//0 is the length of the timeout
      
    if (fdArr[0].revents & POLLIN) { //read from the socket
      //receive 1st packet of data
  	  bzero(packet1.data,DATA_SIZE);
  	  n = recvfrom(sockfd, &packet1, PACKETSIZE, 0, &serveraddr, &serverlen);
  	  if (n < 0) 
        error("ERROR in recvfrom packet 1");
 
   	  fprintf(stderr, "Receiving packet %d\n",packet1.seq_num);
  	  //check if already ACKed
  	  if (packet1.seq_num < rcv_base) { //Just retransmit the ACK don't get any data
  	    //send ACK Packet
  	    struct TCP_PACKET ACK1;
  		ACK1.seq_num = client_seqnum;
 	 	ACK1.window_size = WINDOW_SIZE;
  		ACK1.ack_num = packet1.seq_num + HEADERSIZE + strlen(packet1.data);
  		bzero(ACK1.data,DATA_SIZE);
  		//send the ACK for packet 1
  		n = sendto(sockfd, &ACK1, sizeof(ACK1), 0, &serveraddr, serverlen);
  		if (n < 0) 
    	  error("ERROR in sendto");
  		fprintf(stderr, "Sending packet %d Retransmission\n",ACK1.ack_num);	
  	  }
  	  else {
  	    //check the found flag
  		if (packet1.FILENOTFOUND_flag == 0) {
    	  file_path = (char*)calloc(strlen(filename)+15,sizeof(char)); // allocate for complete filepath including directory (should check the return value)
    	  snprintf(file_path,15,"%s",dir);//copy dir name
    	  strcat(file_path, filename); // add filename 
    	  /* Store the server's reply in the file*/
    	  fd = open(file_path,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR);
    	  write(fd,packet1.data,strlen(packet1.data));
  		}
  		else {fileFound = 0;}
 		
 		fileDetermined = 1;

  		//send ACK Packet
  	    struct TCP_PACKET ACK1;
  		ACK1.seq_num = client_seqnum;
 	 	ACK1.window_size = WINDOW_SIZE;
  		ACK1.ack_num = packet1.seq_num + HEADERSIZE + strlen(packet1.data);
  		bzero(ACK1.data,DATA_SIZE);
  		//send the ACK for packet 1
  		n = sendto(sockfd, &ACK1, sizeof(ACK1), 0, &serveraddr, serverlen);
  		if (n < 0) 
    	  error("ERROR in sendto for lastsynSegment");
  		fprintf(stderr, "Sending packet %d\n",ACK1.ack_num);
  		rcv_base = ACK1.ack_num;
      }
    }
  }

  if (!fileFound) {
    fprintf(stderr,"404 NOT FOUND\n"); //display 404 not found
  }
  
  else { //file was found and more packets might be coming
    int numDataLastPacket = strlen(packet1.data);
    struct pollfd fdArr[1];
    fdArr[0].fd = sockfd;
    fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR
    while(numDataLastPacket == DATA_SIZE-1) { //indicating the last packet had full data packet, assuming at least one more packet lest not necessairly true
      //use poll to wait for a packet, read the packet and create an ACK for it   
      poll(fdArr,1,0);//0 is the length of the timeout
      
      if (fdArr[0].revents & POLLIN) { //read from the socket
        //receive Packet with data
 	    struct TCP_PACKET packet;
 	    bzero(packet.data,DATA_SIZE);
 	    n = recvfrom(sockfd, &packet, PACKETSIZE, 0, &serveraddr, &serverlen);
 	    if (n < 0) 
  	      error("ERROR in recvfrom packet 1");
 	    
 	    fprintf(stderr, "Receiving packet %d\n",packet.seq_num);
 	    
 	    if (packet.seq_num < rcv_base) { //just send the ACK
 	      struct TCP_PACKET ACK;
 	      ACK.ack_num = packet.seq_num + HEADERSIZE + strlen(packet.data);
 	      bzero(ACK.data,DATA_SIZE);
 	      //send the ACK for packet
 	      n = sendto(sockfd, &ACK, sizeof(ACK), 0, &serveraddr, serverlen);
 	      if (n < 0) 
  	        error("ERROR in sendto for ACK Segment");
 	      fprintf(stderr, "Sending packet %d Retransmission\n",ACK.ack_num);
 	      numDataLastPacket = strlen(packet.data);
 	      fprintf(stderr, "numDataLastPacket:%d\n",strlen(packet.data));
 	    }

 	    else { //client is receiving new data so save it and update the rcv_base
 	      write(fd,packet.data,strlen(packet.data)); //write the data to the packet, we are assuming reliable delivery of in order packets
 	      //send ACK Packet
 	      struct TCP_PACKET ACK;
 	      ACK.ack_num = packet.seq_num + HEADERSIZE + strlen(packet.data);
 	      bzero(ACK.data,DATA_SIZE);
 	      //send the ACK for packet
 	      n = sendto(sockfd, &ACK, sizeof(ACK), 0, &serveraddr, serverlen);
 	      if (n < 0) 
  	        error("ERROR in sendto for ACK Segment"); 	    
 	      fprintf(stderr, "Sending packet %d\n",ACK.ack_num);
 	      numDataLastPacket = strlen(packet.data);
 	      fprintf(stderr, "numDataLastPacket:%d\n",strlen(packet.data));
		  rcv_base = ACK.ack_num;
		}
      }
    }
    close(fd);
  }
 return 0;
}