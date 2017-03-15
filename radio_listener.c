/*
 * radio_listener.c
 *
 *  Created on: Jan 3, 2016
 *      Author: ubu
 */


#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>

#define BUFLEN 1024


int main(int argc,char* argv[])
{
  char buf[BUFLEN];
  struct sockaddr_in si_other;
  struct ip_mreq mcg;
  int s, i, slen=sizeof(si_other);
  uint16_t port;

  if(argc!=3) {printf("wrong number of arguments(%d)\n", argc); exit(1);}
  	port=atoi((char*)argv[2]);

  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
    perror("socket");
    exit(1);
  }

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(port);
  si_other.sin_addr.s_addr=htonl(INADDR_ANY);

  mcg.imr_interface.s_addr = htonl(INADDR_ANY);
  mcg.imr_multiaddr.s_addr = inet_addr(argv[1]);
  if(setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,(struct ip_mreq *)&mcg, sizeof(mcg)) == -1) {
  	close(s);
  	perror("Error while trying to join multicast group");
  	exit(1);
  }


  if (bind(s, &si_other, sizeof(si_other))==-1){
     perror("bind");
     exit(1);
  }
  while(1){
    if ((i=recvfrom(s, buf, BUFLEN, 0, &si_other, &slen))==-1){
      perror("recvfrom");
      exit(1);
    }
    if (write(1, buf, i)<0){
      perror("write");
      exit(1);
    }
  }

  close(s);
  return 0;
}
