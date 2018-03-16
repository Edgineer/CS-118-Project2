//Thread Saftey
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

struct packet_check {
	int isACKed;
	int expectedACK;
};

struct packet_check *tempPacketMap;

int main(int argc, char const *argv[]) {

  FILE *file = NULL;
  char filedata[1000];
  bzero(filedata, 1000);
  size_t bytesRead = 0;
  int numTotalPackets = 6496; //get the total number of packets needed for data transmission
  file = fopen("big.txt", "r");
  if (file == NULL)
    perror("opening file to read");
  
  FILE *wfile = NULL;
  wfile = fopen("mybig.txt", "w+");

  for (int i = 0; i < numTotalPackets; ++i) {
    bytesRead = fread(filedata, 1, 999, file);
    fwrite(filedata,bytesRead,1,wfile);
    bzero(filedata,1000);
  }
	
  //setup the global packetmap
  struct packet_check *packetMap = realloc(tempPacketMap,sizeof(struct packet_check)*numTotalPackets);

  for (int i = 0; i < numTotalPackets; ++i) {
    packetMap[i].isACKed = 1;
	packetMap[i].expectedACK = i;
  }

  for (int i = 0; i < numTotalPackets; ++i) {
    fprintf(stderr, "Packet #%d: isACKed=%d expectedACK=%d\n",i+100,packetMap[i].isACKed,packetMap[i].expectedACK);
  }

  fclose(file);
  free(packetMap);
  return 0;
}
