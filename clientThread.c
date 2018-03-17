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

//Packet will contain the number of bytes for the file
struct TCP_PACKET rcv_buffer[5];

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

//initial client sequence number
int client_seqnum = 10;

int rcv_base = 0; //should be equal to the server initial sequence number
int fd = 0; //file descriptor
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

  // check arguments
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

  /**
   *
   *  Handshake
   * 
   *  Assume SYN segment has been sent and SYNACK segment has been received
   *  rcv_base = server initial sequence number (42)
   *  Send the last SYN segment containing the file request
   *
   */
  rcv_base = 43;

  //Create final SYN Segment & piggyback request
  struct TCP_PACKET lastsynSegment;
  lastsynSegment.seq_num = client_seqnum+1; //SYN segment has initial client_seqnum, don't really care
  lastsynSegment.ack_num = 43; //should be the SYNACK packet sequence number + 1
  lastsynSegment.window_size = WINDOW_SIZE;
  bzero(lastsynSegment.data,DATA_SIZE);
  snprintf(lastsynSegment.data,strlen(filename)+1, "%s",filename);//copy dir name +1 for null byte
  
  //send the final SYN Segment with the filename
  n = sendto(sockfd, &lastsynSegment, PACKETSIZE, 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto for lastsynSegment");
  client_seqnum++;
  fprintf(stderr,"Sending packet %d\n",lastsynSegment.ack_num);

  /*
  *
  * End of Connection Handshake
  *
  */

  //Assuming we'll receive the first packet indicating if the file was found correctly and has number of bytes for the file
  struct TCP_PACKET packet1;
  bzero(packet1.data,DATA_SIZE);
  n = recvfrom(sockfd, &packet1, PACKETSIZE, 0, &serveraddr, &serverlen);
  if (n < 0) 
    error("ERROR in recvfrom packet 1");
   
  fprintf(stderr, "Receiving packet %d\n",packet1.seq_num);
  
  //send ACK Packet
  struct TCP_PACKET ACK1;
  ACK1.seq_num = client_seqnum;
  ACK1.window_size = WINDOW_SIZE;
  ACK1.ack_num = packet1.seq_num + HEADERSIZE + strlen(packet1.data);
  bzero(ACK1.data,DATA_SIZE);
  n = sendto(sockfd, &ACK1, sizeof(ACK1), 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto");
  printf(stderr, "Sending packet %d\n",ACK1.ack_num);
  rcv_base = ACK1.ack_num; 
  
  //Assuming we are not here until the server get the last ACK
  if (packet1.FILENOTFOUND_flag == 0) { //file was found and more packets might be coming

    int totalNumBuytes = atoi(packet1.data);
    int numTotalPackets = totalNumBuytes/999; //get the total number of packets needed for data transmission
    if ((totalNumBuytes%(999)!=0)) {numTotalPackets++;}

    int finalSeqNum = rcv_base + totalNumBuytes + (HEADERSIZE*numTotalPackets);
    fprintf(stderr, "client thinks final sequence number is%d\n",finalSeqNum);

    file_path = (char*)calloc(strlen(filename)+15,sizeof(char)); // allocate for complete filepath including directory (should check the return value)
    snprintf(file_path,15,"%s",dir);//copy dir name
    strcat(file_path, filename); // add filename
    fd = open(file_path,O_CREAT|O_RDWR,S_IRUSR|S_IWUSR);

    //Initialize the Buffer
    for (int i = 0; i < 5; ++i) {
      rcv_buffer[i].seq_num = -1;
      bzero(rcv_buffer[i].data,DATA_SIZE);
    }
    
    struct pollfd fdArr[1];
    fdArr[0].fd = sockfd;
    fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR
    
    fprintf(stderr, "receiveBase:%d\n",rcv_base);

    while(rcv_base < finalSeqNum) { //indicating the last packet had full data packet, assuming at least one more packet left. Not gauranteed
      
      //look for a sequence number == rcv_base in the receiving buffer
      //if you find one write the data to the file and increment the rcv base by amount of data in the packet
      for (int i = 0; i < 5; ++i) { //Iterate buffer and increase the rcv_base as needed
        if (rcv_base == rcv_buffer[i].seq_num) { //the last unAcked packet has now been ACKed
          rcv_base = rcv_buffer[i].seq_num + HEADERSIZE + strlen(rcv_buffer[i].data);
          write(fd,rcv_buffer[i].data,strlen(rcv_buffer[i].data)); //write ordered data
        }
      }

      //use poll to wait for a packet, read the packet and create an ACK for it   
      poll(fdArr,1,0);//0 is the length of the timeout
      
      if (fdArr[0].revents & POLLIN) { //received a packet
        struct TCP_PACKET packet;
        bzero(packet.data,DATA_SIZE);
        n = recvfrom(sockfd, &packet, PACKETSIZE, 0, &serveraddr, &serverlen);
        if (n < 0) 
          error("ERROR in recvfrom packet 1");
      
        fprintf(stderr, "Receiving packet %d\n",packet.seq_num);

        if (packet.seq_num < rcv_base) { //receiving a duplicate packet just ACK
          struct TCP_PACKET dupACK;
          dupACK.ack_num = packet.seq_num + HEADERSIZE + strlen(packet.data);
          bzero(dupACK.data,DATA_SIZE);
          //send the ACK for packet
          n = sendto(sockfd, &dupACK, PACKETSIZE, 0, &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto for ACK Segment");
          fprintf(stderr, "Sending packet %d Retransmission\n",dupACK.ack_num);
        }

        else { //client is receiving new data so save it and update the rcv_base
            //this is a new packet and we must loop through the buffer
              //iterate through the packets to find one with the sequence number < rcv_base
              //replace this index with the new packet
          for (int i = 0; i < 5; ++i) {
            if (rcv_buffer[i].seq_num < rcv_base) { //found old packet
              rcv_buffer[i].seq_num = packet.seq_num;
              rcv_buffer[i].ack_num = packet.ack_num;
              //maybe add the other member variables
              bzero(rcv_buffer[i].data,DATA_SIZE);//Sure the data was already written to the file?
              snprintf(rcv_buffer[i].data,strlen(packet.data)+1,"%s",packet.data);
              break;
            }
          }
          //send ACK Packet
          struct TCP_PACKET ACK;
          ACK.ack_num = packet.seq_num + HEADERSIZE + strlen(packet.data);
          bzero(ACK.data,DATA_SIZE);
          //send the ACK for packet
          n = sendto(sockfd, &ACK, sizeof(ACK), 0, &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto for ACK Segment");       
          fprintf(stderr, "Sending packet %d\n",ACK.ack_num);
        }
      }
    }
    close(fd);
  }

  /*
  *
  *
  * Close Connection Protocol
  *
  *
  */
 return 0;
}