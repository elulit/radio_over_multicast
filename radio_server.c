/*
 ============================================================================
 Name        : server.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define BUFLEN 100
#define minCostumers 100
#define typeError 2
#define typeAnnounce 1
#define typeWelcome 0
//Structures declaration

typedef struct SocketUDP{
	char Song[100];
	uint16_t Port;
	int* stations;
	struct in_addr mcgAddr;
}SocketUDP;

typedef struct SocketTCP{
	int socket, number;
	int* stations;
}SocketTCP;


//functions
void openUDP(void* station);
void serverHandler();
void openTCP(void* client);
int sendMessage (int sock, int Type, int station, char* str);
void deleteClient(int serial);



int numStation,numClients=1,s,newClientSock;
char songsNames[100][100];
uint16_t UDPport,TCPport;
uint32_t multicastAddress;
struct in_addr Clients[minCostumers];




int main(int argc,char* argv[]) {
	char* MultiCastAdr[1];
	SocketUDP StationsSocket[30];
	SocketTCP *clientTCP,*x;
	int i,t,stationsCounter[argc-4];
	unsigned char numbersIP[4];
	pthread_t UDPthreads[numStation],serverUI,TCPtheard;
	struct hostent* multicastIP;
	struct sockaddr_in si_other,client, multicastGroupIP;
	struct in_addr temp;

	/*creating multicast in udp*/
	if(argc>=5){
		numStation=argc - 4;
		UDPport = atoi((char*)argv[3]);
		TCPport = atoi((char*)argv[1]);
		MultiCastAdr[0] = argv[2];
		multicastAddress = inet_addr(MultiCastAdr[0]);

		for(i=4; i<argc;i++)
			strcpy(songsNames[i-4],argv[i]);

		if ((multicastIP=gethostbyname(argv[2]))==NULL)
		{
			puts("ERROR: illegal IP address.\n");
			exit(1);
		}//wrong ip address

		memcpy((char*)&multicastGroupIP.sin_addr.s_addr, multicastIP->h_addr_list[0],multicastIP->h_length);

		if(!(IN_MULTICAST(ntohl(multicastGroupIP.sin_addr.s_addr)))) {
			puts("ERROR: illegal Multicast IP address.\n");
			exit(1);
		}//not a multicast address

		puts("starting multicast!\nstations number and addresses:");
		numbersIP[0] = (multicastAddress >> 24) & 0xFF;
		numbersIP[1] = (multicastAddress >> 16) & 0xFF;
		numbersIP[2] = (multicastAddress >> 8) & 0xFF;
		numbersIP[3] = multicastAddress & 0xFF;

		for (i=0;i<numStation;i++)
			printf("%d: %d.%d.%d.%d\n",i,numbersIP[3],numbersIP[2],numbersIP[1],(numbersIP[0]+i));
		puts("");

		inet_aton(argv[2],&temp);
		//creating the broadcasts
		for(i=0;i<numStation;i++){
			strcpy(StationsSocket[i].Song,songsNames[i]);
			StationsSocket[i].Port=UDPport;
			StationsSocket[i].mcgAddr=temp;
			StationsSocket[i].stations = stationsCounter;
			temp.s_addr = htonl(temp.s_addr)+1;
			temp.s_addr = htonl(temp.s_addr);
			if(pthread_create(&UDPthreads[i], NULL, openUDP, (void*)&StationsSocket[i])) {
				perror("pthread_create()");
				printf("1");
				exit(1);
			}//Udp's threads creating.
		}//for
	}
	else{
		puts("ERROR: wrong number of arguments.\n");
		exit(1);
	}//else

	/*Handling TCP connections*/

	if ((s=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))==-1){
		perror("ERRORL: opening socket");
		exit(1);
	}   // creating a socket

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;                                // creating a matching sockaddr variable
	si_other.sin_port = htons(TCPport);
	si_other.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(s, &si_other, sizeof(si_other))<0){
		puts("ERROR: bind()");
		close(s);
		exit(1);
	}

	if(listen(s,minCostumers)!=0){
		perror("listen()");
		close(s);
		exit(1);
	} //wait for connection

	if(pthread_create(&serverUI, NULL, serverHandler, NULL))
	{
		perror("thread");
		close(s);
		//	close(clientSock);
		exit(1);
	} //User's thread creating.


	t=sizeof(client);
	while(0!=(newClientSock=accept(s, (struct sockadrr *)&client, &t))){//got new connection
		printf("Client number %d connected. Expecting for HELLO message...\n", numClients);
		//creating the user information
		clientTCP = (SocketTCP *)malloc(sizeof(SocketTCP));
		clientTCP->socket = newClientSock;
		clientTCP->stations = stationsCounter;
		clientTCP->number = numClients;
		Clients[numClients-1] = client.sin_addr;
		numClients++;
		if(pthread_create(&TCPtheard, NULL, openTCP, (void *)&clientTCP)){//create new thread to treat the new tcp client
			perror("thread");
			close(s);
			close(newClientSock);
			exit(1);
		}//Tcp's connections thread crashed.
	}
	if(newClientSock<0) {
		perror("Socket()");
		close(s);
		exit(1);
	}

	free(clientTCP);
	close(s);

	return 0;

}




void openUDP(void* station){
	SocketUDP *Multicast = (SocketUDP *)station;
	int sock, readSongBytes;
	struct sockaddr_in multicastAddr;
	FILE *song;
	char *songName;
	char string[10], buff[1024];
	unsigned char ttl;

	if ((sock=socket(AF_INET, SOCK_DGRAM, 0))==-1){
		perror("socket()");
		exit(1);
	}// creating a socket


	memset((char *) &multicastAddr, 0, sizeof(multicastAddr));
	memset(buff, 0, 1024);
	multicastAddr.sin_family = AF_INET;
	multicastAddr.sin_port = htons(Multicast->Port);
	multicastAddr.sin_addr.s_addr = Multicast->mcgAddr.s_addr;

	ttl = 15;		 //Generating number of hops
	if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl))==-1){
		perror("setsockop():");
		close(sock);
	}

	if ((song = fopen(Multicast->Song, "r"))==NULL){//open the song
		perror("fopen():");
		close(sock);
		close(s);
		exit(1);
	}   // Open song's file

	while(1){
		if((readSongBytes=fread(buff, 1, 1024, song)) < 0 ) {
			perror("fread():");
			close(sock);
			close(s);
			exit(1);
		}//transmit it to buffer

		while (readSongBytes > 0) {
			if (sendto(sock, buff, readSongBytes, 0, (struct sockaddr *) &multicastAddr, sizeof(multicastAddr))<0) {
				perror("sendto():");
				close(sock);
				close(s);
				exit(1);
			}//failed

			memset(buff, 0, 1024);
			usleep(36000);

			// Streaming song
			if (readSongBytes<1024){
				rewind(song);//start over
			}
			readSongBytes=0;
		}//while
	}//while

}

void serverHandler(){
	int i;
		fd_set set;
		char buf[BUFLEN];

		while(1){
			FD_ZERO(&set);
			FD_SET(0, &set);
			i = select(1, &set, NULL, NULL, 0);                      // Wait for an answer from the server

			if (i==-1) {
				perror("select()");
				close(s);
				exit(1);
			}

			if (FD_ISSET(0, &set)){
		  		memset((void*)&buf, 0, 2);
		  		if((i=read(0, buf, 2))<0) { // Entering if receive from the stdin
		  			perror("read()");
		  			close(s);
		  			exit(1);
		  		}
		  		else if(i==0) {
		  			close(s);
		  			exit(1);
		  		}

		  		if (buf[0]=='q') {
		  			for (i=0; i<numStation; i++)
		  				printf("Closing station number %d\n",i);
		  			close(s);
		  			exit(1);
		  		}
				if (buf[0]=='p') {
					puts("Printing Databsae:");
					for (i=0; i<numStation; i++){
						printf("Station %d playing '%s'\n",i,songsNames[i]);
					}//for
					puts("Clients connected:");
					if (numClients>1){
						for (i=0; i<(numClients-1); i++){
							printf("Client %d IP: %s\n", (i+1), inet_ntoa(Clients[i]));
						}//for
					}//if
					else {puts("None.");}
				}//if
		  		else{puts("Wrong input. Please try again\n");}
		  	}//if
		}//while
}



void openTCP(void* client){
	SocketTCP *currentClient = (SocketTCP *)client;
	int i,sock;
	int clientSerial = numClients-1;
	fd_set set;
	uint16_t reserved,stationNumber;
	struct timeval timeout;
	char buf[BUFLEN];

	sock=*(int*)currentClient->socket;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	timeout.tv_sec=0;
	timeout.tv_usec=500000; //0.5 seconds

	i = select(sock+1, &set, NULL, NULL, &timeout);                      // Wait for an answer from the client

	if (i==-1) {//error
		perror("select()");
		close(s);
		close(sock);
		return;
	}
	if (i==0) {//Timeout
		printf("Timeout. Disconnect client!\n");
		close(sock);
		return;
	}
	if (i>0){//got answer
		memset((void*)&buf, 0, BUFLEN);//reset the buff
		i = recv(sock, &buf, BUFLEN, 0);
		if (i==-1) {
			perror("recv()");
			close(sock);
			close(s);
			return;
		}

		if (i==0) {
			puts("No answer. Disconnecting client!\n");
			close(sock);
			return;
		}
		if (buf[0]==1) {
			if(sendMessage(sock, typeError,0,"An ASK-SONG request was sent before receiving a HELLO message")==0){
				perror("Send()");
				close(sock);
				return;
			}
		}

		if (buf[0]==0){
			reserved=((uint16_t)(buf[1])*256+(uint8_t)buf[2]);
			if(!reserved){
				puts("HELLO received. Sending WELCOME message!");   //Received proper HELLO message
				sendMessage(sock, typeWelcome, 0,NULL);
			}

		}
		else {
			sendMessage(sock, typeError,0,"Unknown command type.");
		}
	}//if(i>0)

	//connection established- waiting for ask songs messages

	while(1){
		FD_ZERO(&set);
		FD_SET(sock, &set);
		FD_SET(0, &set);
		i = select(sock+1, &set, NULL, NULL, 0);                      // Wait for an answer from the server
		if (i==-1) {
			perror("select()");
			close(s);
			close(sock);
			return;
		}

		if(FD_ISSET(sock, &set)){
			memset((void*)&buf, 0, BUFLEN);
			i = recv(sock, &buf, BUFLEN, 0);
			if (i==-1) {
				perror("recv()");
				deleteClient(clientSerial);
				numClients--;
				close(sock);
				return;
			}

			if (i==0) {
				printf("Session %d: Client disconnected!\n", clientSerial);
				deleteClient(clientSerial);
				numClients--;
				close(sock);
				return;
			}

			if (buf[0]==0)
				sendMessage(sock, typeError, 0, "Wrong message receive");

			if (buf[0]==1){
				printf("Session %d: An ASK-SONG requset received. Sending ANNOUNCE.\n", clientSerial);
				stationNumber=((uint16_t)(buf[1])*256+(uint8_t)buf[2]);
				if (stationNumber >= numStation || stationNumber<0){//invalid station number
					printf("Session %d: An illegal ASK-SONG requset received. Sending INVALID_COMMAND.\n", clientSerial);
					sendMessage(sock, typeError, stationNumber, "This station's number doesn't exist!");
				}
				else
					sendMessage(sock, typeAnnounce, stationNumber, NULL);
			}//if
			else {sendMessage(sock, typeError, 0, "Unknown command type");}
		}//if

		if (FD_ISSET(0, &set)){
			memset((void*)&buf, 0, 2);
			if((i=read(0, buf, 2))<0) {perror("read()"); close(s); exit(1);}     // Entering if receive from the stdin
			else if(i==0) {close(s); exit(1);}
			if (buf[0]=='q') {
				for (i=0; i<numStation; i++)
					printf("Closing station number %d\n",i);
				close(s);
				exit(1);
			}
			if (buf[0]=='p') {
				for (i=0; i<numStation; i++){
					printf("Station %d playing '%s'\n",i,songsNames[i]);
				}//for
				puts("Clients connected:");
				if (numClients>1){
					for (i=0; i<(numClients-1); i++)
						printf("Client %d IP: %s\n", (i+1), inet_ntoa(Clients[i]));
				}//if
				else {puts("None.");}
			}//if
			else{puts("Wrong input. Please try again\n");}
		}//if
	}//while
}//openTCP



int sendMessage (int sock, int Type, int station, char* str){
	unsigned char buf[BUFLEN];
	uint8_t size;

	switch(Type){
	case 0://hello
		buf[0]=(uint8_t)Type;
		buf[1]=(numStation >> 8) & 0xFF;
		buf[2]=numStation & 0xFF;
		buf[3]=(multicastAddress >> 24) & 0xFF;
		buf[4]=(multicastAddress >> 16) & 0xFF;
		buf[5]=(multicastAddress >> 8) & 0xFF;
		buf[6]=multicastAddress & 0xFF;
		buf[7]=(UDPport >> 8) & 0xFF;
		buf[8]=UDPport & 0xFF;
		if(send(sock, (void *)buf, sizeof(uint8_t)*9, 0)==-1){
			perror("send():");
			return 0;
		}
		break;

	case 1://announce
		buf[0]=(uint8_t)Type;
		size=strlen(songsNames[station]);
		buf[1]=(uint8_t)size;
		memcpy((char*)&buf[2], songsNames[station], strlen(songsNames[station]));
		if(send(sock, buf,sizeof(uint8_t)*(size+2), 0)==-1){
			perror("send():");
			return 0;
		}
		break;

	case 2://error
		buf[0]=(uint8_t)Type;
		size=strlen(str);
		buf[1]=(uint8_t)size;
		memcpy((char*)&buf[2], str, size);
		buf[size+2]='\0';
		printf("%s\n", buf);
		if(send(sock, buf,sizeof(uint8_t)*(size+2), 0)==-1) {
			perror("send():");
			return 0;
		}
		break;
	}

	return 1;
}

void deleteClient(int serial){
	int i;
	char *temp;

	if (serial<numClients && serial!=1){
		for (i=serial; i<numClients; i++){
			strcpy(temp, inet_ntoa(Clients[i]));
			Clients[i-1].s_addr = inet_addr(temp);
		}//for
	}//if
}


