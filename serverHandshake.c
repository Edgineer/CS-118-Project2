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
  int expectedACK;
};

struct packet_info {
  struct TCP_PACKET datapacket;
  int packetNumber;
  int *expectedACKptr;
  int *isACKedptr;
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
#include <pthread.h>

#define HEADERSIZE 24
#define PACKETSIZE 1024
#define DATASIZE 1000
#define MAX_SEQUENCE_NUMBER 30720
#define WINDOW_SIZE 5120
#define RTO 500//ms

int numTotalPackets = 1;
int send_base = 42;
int nextseqnum = 42;
int clientACK = 0;
int numSentUnacked = 0;
int receivedFIN = 0;
int sockfd, clientlen; //client's address size
struct sockaddr_in serveraddr, clientaddr;

struct packet_check *tempPacketMap;
struct packet_check *packetMap;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void error(char *msg) {
  perror(msg);
  exit(1);
}

//thread function
void *handlePacket(void *arg) {
  if(pthread_mutex_lock(&lock)!=0){error("Error aqcuiring lock!");}

  struct packet_info *packet = (struct packet_info *) arg;
  struct TCP_PACKET outPacket = packet->datapacket;
  int numPacket = packet->packetNumber;
  int *expectedACK = packet->expectedACKptr;
  int *isACKed = packet->isACKedptr;

 // fprintf(stderr, "in the thread packet number is %d\n",numPacket);
  
  struct pollfd fdArr[1];
  fdArr[0].fd = sockfd;
  fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR

  //send the packet
  int n = sendto(sockfd, &outPacket, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
  if (n < 0) 
    error("ERROR in sendto");
  fprintf(stderr, "Sending packet %d %d %s\n",outPacket.seq_num,outPacket.window_size);
  
  time_t start_t;
  time(&start_t);

  //check for a FIN packet
 // fprintf(stderr, "initial ack value:%d, expectedACK is:%d\n",*isACKed,*expectedACK);
  *isACKed = 1;
  *expectedACK = outPacket.seq_num + HEADERSIZE + strlen(outPacket.data);
 // fprintf(stderr, "post ack value:%d, expectedACK is:%d\n",*isACKed,*expectedACK);
  *isACKed = 0;

  while(*isACKed == 0) {
    time_t end_t;
    time(&end_t);
    if (difftime(end_t,start_t) > 0.5) { //retransmit the packet
      n = sendto(sockfd, &outPacket, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
      if (n < 0)
        error("ERROR in sendto");
      fprintf(stderr, "Sending packet %d %d Retransmission\n",outPacket.seq_num,outPacket.window_size);
      time(&start_t); 
    }

    //use poll to wait for a packet, Extract packet data
    poll(fdArr,1,0);//0 is the length of the timeout
    if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
      struct TCP_PACKET ACK;
      n = recvfrom(sockfd, &ACK, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
      if (n < 0)
          error("ERROR in recvfrom ACK");
     // fprintf(stderr, "receiving ACK number is:%d expected ACK number is:%d\n",ACK.ack_num,*expectedACK);
      if (ACK.ack_num == *expectedACK) {
        *isACKed = 1;
        send_base = ACK.ack_num;//will not set value correctly
        fprintf(stderr, "Receiving packet %d\n",ACK.ack_num);
      }
      //if (ACK.FIN_flag==1) {receivedFIN = 1;}
    }
  }
  numSentUnacked--;
  if(pthread_mutex_unlock(&lock)!=0){error("Error releasing lock!");}
  pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
  int portno;
  int n;
  struct stat fileStats;

  struct hostent *hostp;
  char *hostaddrp;
  int optval;

  if (argc != 2) {
    fprintf(stderr, "usage: %s < portnumber >\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  //create socket with UDP protocol
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

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
    send_base = 42;
    nextseqnum = 42; //you sent out the SYNACK so nextseqnum by 1

    /**
     *
     *  Handshake
     * 
     *  Assume SYN segment has been received and SYNACK segment has been sent
     *  Next_seqnum = server initial sequence number (42)
     *  Receive the last SYN segment containing the filename
     *
     */

    //Receive the SYN
    struct pollfd fdArr[1];
    fdArr[0].fd = sockfd;
    fdArr[0].events = (POLLIN);//POLLIN+POLLHUP+POLLERR   

    struct TCP_PACKET getSYN;
    int SYNreceived = 0;
    while(SYNreceived == 0){
      //use poll to wait for a packet, Extract packet data
      poll(fdArr,1,0);//0 is the length of the timeout
      if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
        n = recvfrom(sockfd, &getSYN, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
          error("ERROR in recvfrom ACK");
        if (getSYN.SYN_flag == 1) {
          clientACK = getSYN.seq_num+1; 
          SYNreceived = 1;
          fprintf(stderr, "Receiving packet SYN\n");
        }
      }
    }

    //Create/Send a SYNACK
    struct TCP_PACKET SYNACK;
    SYNACK.seq_num = nextseqnum;
    SYNACK.ack_num = clientACK;
    SYNACK.SYN_flag = 1;
    SYNACK.FILENOTFOUND_flag = 0;
    SYNACK.window_size = WINDOW_SIZE;
    bzero(SYNACK.data,DATASIZE);

    nextseqnum += 1;
    
    //Create the packet check object
    struct packet_check packSYNACK;
    packSYNACK.isACKed=0;
    packSYNACK.expectedACK = nextseqnum;
  
    n = sendto(sockfd, &SYNACK, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
    if (n < 0)
      error("ERROR in sendto");
    time_t SYNACKstart_t;
    time(&SYNACKstart_t);
    fprintf(stderr, "Sending packet %d %d SYN\n",SYNACK.seq_num,SYNACK.window_size);
    
    struct TCP_PACKET lastsynSegment;

    while(packSYNACK.isACKed == 0) {
      time_t SYNACKend_t;
      time(&SYNACKend_t);
      if (difftime(SYNACKend_t,SYNACKstart_t) > 0.5) { //retransmit the packet
        n = sendto(sockfd, &SYNACK, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
          error("ERROR in sendto");
        fprintf(stderr, "Sending packet %d %d SYN Retransmission\n",SYNACK.seq_num,SYNACK.window_size);
        time(&SYNACKstart_t); 
      }

      //use poll to wait for SYNACK, Extract packet data
      poll(fdArr,1,0);//0 is the length of the timeout
      if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
        n = recvfrom(sockfd, &lastsynSegment, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
          error("ERROR in recvfrom ACK");
        if (lastsynSegment.ack_num == packSYNACK.expectedACK) {
          packSYNACK.isACKed = 1;
          clientACK = lastsynSegment.seq_num + 1;
          fprintf(stderr, "Receiving packet %d\n",lastsynSegment.ack_num);
          send_base = lastsynSegment.ack_num;
        }
      }
    }  
    /*
    *
    * End of Connection Handshake
    *
    */
    //gauranteed to receive the client's request
    if (stat(lastsynSegment.data, &fileStats) == 0) { //file found in the server's wd
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////
      //Create the dataInfo Packet
      struct TCP_PACKET dataInfo;
      dataInfo.seq_num = nextseqnum;
      dataInfo.ack_num = clientACK;
      dataInfo.FILENOTFOUND_flag = 0;
      dataInfo.window_size = WINDOW_SIZE;
      bzero(dataInfo.data,DATASIZE);
      snprintf(dataInfo.data,42, "%d",fileStats.st_size);
      //nextseqnum = nextseqnum + HEADERSIZE + strlen(dataInfo.data);
      
      nextseqnum += 1;

      //Create the packet check object
      struct packet_check packDataInfo;
      packDataInfo.isACKed = 0;
      packDataInfo.expectedACK = nextseqnum;

      n = sendto(sockfd, &dataInfo, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
      if (n < 0)
        error("ERROR in sendto");
      fprintf(stderr, "Sending packet %d %d\n",dataInfo.seq_num,dataInfo.window_size);
  
      time_t datastart_t;
      time(&datastart_t);

      struct TCP_PACKET dataACKed;
      //fprintf(stderr, "data info expect#%d\n",packDataInfo.expectedACK);
      while(packDataInfo.isACKed == 0) {
        time_t dataend_t;
        time(&dataend_t);
        if (difftime(dataend_t,datastart_t) > 0.5) { //retransmit the packet
          n = sendto(sockfd, &dataACKed, PACKETSIZE, 0,(struct sockaddr *) &clientaddr, clientlen);
          if (n < 0)
            error("ERROR in sendto");
          time(&datastart_t); 
        }

        //use poll to wait for SYNACK, Extract packet data
        poll(fdArr,1,0);//0 is the length of the timeout
        if (fdArr[0].revents & POLLIN) { //ACK arrived, read from the socket
          n = recvfrom(sockfd, &dataACKed, PACKETSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
          if (n < 0)
            error("ERROR in recvfrom ACK");
         // fprintf(stderr, "ack packet ack#%d data info expecte #%d SYN Retransmission\n",dataACKed.ack_num,packDataInfo.expectedACK);
          if (dataACKed.ack_num == packDataInfo.expectedACK) {
            packDataInfo.isACKed = 1;
            fprintf(stderr, "Receiving packet %d\n",dataACKed.ack_num);
            send_base = dataACKed.ack_num;
          }
        }
      }

      FILE *file = NULL;
      char filedata[DATASIZE];
      bzero(filedata, DATASIZE);
      size_t bytesRead = 0;
      numTotalPackets = (long long) fileStats.st_size/(DATASIZE-1); //get the total number of packets needed for data transmission
      if ((fileStats.st_size%(DATASIZE-1)!=0)) {numTotalPackets++;}
     // fprintf(stderr, "Total number of packets = %d\n",numTotalPackets);
      file = fopen(lastsynSegment.data, "r");
      if (file == NULL)
        error("opening file to read");

      //allocate packetMap
      struct packet_check *packetMap = realloc(tempPacketMap,sizeof(struct packet_check)*numTotalPackets);

      //initialize packetMap 
      for (int i = 0; i < numTotalPackets; ++i) {
        int expected = nextseqnum + (i*(PACKETSIZE-1));
        if (i == numTotalPackets-1 && fileStats.st_size%(DATASIZE-1) != 0) {
          expected = nextseqnum + ((i-1)*(PACKETSIZE-1)) + (fileStats.st_size%(DATASIZE-1));
        }
        packetMap[i].isACKed = 0;
        packetMap[i].expectedACK = expected;
      }

      for (int i = 0; i < numTotalPackets; ++i) {
       // fprintf(stderr, "for packet %d expected ACK is:%d\n",i,packetMap[i].expectedACK);
      }

      //create thead array
      pthread_t thread_arr[numTotalPackets];
      int numThread = 0;

      int allACKed = 0;
      
    //  fprintf(stderr, "Before the data sending loop\n");
      int inloop = 1;
      while(!allACKed && !receivedFIN) {
        if (numSentUnacked < 1) {
       //   fprintf(stderr, "In the if %d\n",inloop);
          inloop++;  
          //create a data packet 
          struct TCP_PACKET dataPacket;
          dataPacket.seq_num = nextseqnum;
          dataPacket.ack_num = lastsynSegment.seq_num + 1;
          dataPacket.window_size = WINDOW_SIZE;
          bzero(dataPacket.data,DATASIZE);
        
          bytesRead = fread(filedata, 1, DATASIZE-1, file);
          if (bytesRead < 0) {error("ERROR reading from file");}
          snprintf(dataPacket.data, bytesRead+1, "%s", filedata);
          bzero(filedata, DATASIZE);
          
          //Create the struct parameter for thread function (packet_info)
          struct packet_info checkPack;
          checkPack.datapacket = dataPacket;
          checkPack.packetNumber = numThread;
          checkPack.expectedACKptr = &packetMap[numThread].expectedACK;
          checkPack.isACKedptr = &packetMap[numThread].isACKed;

          nextseqnum += (HEADERSIZE + strlen(dataPacket.data));
          numSentUnacked++;

       //   fprintf(stderr, "num of sent Unack'ed:%d\n",numSentUnacked);
            
      //    fprintf(stderr, "num thread before:%d\n",numThread);

          //MAKE A THREAD TO SEND THE PACKET OVER
          if(pthread_create(&thread_arr[numThread],NULL,handlePacket,&checkPack)!=0){
            error("Error while creating threads!");
          }
          numThread++;
       //   fprintf(stderr, "num thread after:%d\n",numThread);
        }                
        //check all are not acked
        int allArrived = 1;
        for (int i = 0; i < numTotalPackets; ++i) {
          if (packetMap[i].isACKed == 0) {allArrived = 0;}
        }
        allACKed = allArrived;
        //on another checking loop go through the packets structure loop if the send_base = the packet's sequence number and it has been ACK'd
        //then increase the send_base and also increase the number of packets sent or is all packets ACK'd then go to close connection mode
      }
    //set sendbase;
    fclose(file);
    free(packetMap);
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
    pack404.expectedACK = nextseqnum + HEADERSIZE + strlen(seg404.data);
  
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
        if (ACK.ack_num == pack404.expectedACK) {
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
  * Close Connection Protocol
  *
  *
  */
  }
}