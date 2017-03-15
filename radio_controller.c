/*
 * radio_controller.c

 *
 *  Created on: Jan 10, 2016
 *      Author:
 * The algorithem idie:
 * 1. Starting Client will intiate an Hello command
 * 2. we will hold a state flag that will help us to know which phase we are at.
 *
 * Hello:
 * 1. State 1 = opens a socket and connects to the server
 * 2. State 2 = Client sends 3 bytes to server (hello) and waits for a welcome.
 * 3. State 3 =Asking the user for a station number and sending it to the server, and waits for the announce.
 * 4. State 4 = an error accord- closing the socket and exiting the progrem.
 * 5. State -1 = closing socket and returning state 1
 *
 * # timeouts
 * #invalid commands/situations
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#define BUFLEN 1024
#define PACKET_SIZE 100
#define WELCOME_LEN 9
#define ANNOUNCE_LEN

typedef struct client_command{
	uint8_t command_type;
	uint16_t data;
}client_command;

int open_socket(char* argv[]);
int send_hello(char* argv[]);
int wait4welcome();
int waitForAnnounce();
int ask_song();
void print_invalid_command(char* buf);
//void diep(char *s);


char buf[BUFLEN],portNumber[2];
int sock=0;
struct sockaddr_in tcp_socket;
uint16_t numStations;

unsigned char multicastGroup[4];



int main(int argc,char* argv[]){
	int state=1;
	int count=0;

	printf("Starting...\n");
	if(argc!=3) {
		printf("Wrong number of arguments.\n");
		exit(1);
	}

	while (state!=-3){
		switch (state){
		case 1:		//Hello
			printf("open socket..\n");//open socket function
			usleep(100000);
			state=open_socket(argv);
			if(count>9)
				state=-2;
			break;
		case 2:				//send hello
			printf("Starting Hello...\n");
			if (send_hello(argv)){
				printf("Starting welcome\n");
				count=0;
				state=wait4welcome(); //return 1 if fails and 2 if succeed
			}
			else{
				count++;
				state=1;
			}
			break;
		case 3:
			//Ask song
			state=ask_song();
			break;
		case 4:
			//waiting for welcome
			state=waitForAnnounce();
			break;
		case -1:
			//error and retry
			close(sock);
			state=1;
			count++;
			break;
		case -2:
			//error and exit
			close(sock);
			exit(1);
			break;
		}
	}
	return 0;
}


int open_socket (char* argv[]){
	struct sockaddr_in tcp_socket;
	struct hostent *host;
	uint16_t port=0;
	char server_addr[50];

	port=atoi((char*)argv[2]);

	if ((sock=socket(AF_INET, SOCK_STREAM, 0))==-1){
		perror("sock");
		return -1;
	}
	/*Using the server address input*/
	strcpy(server_addr, argv[1]);
	printf("Connecting to server %s, with port %d ...\n", server_addr, port);

	memset(&tcp_socket, 0, sizeof(struct sockaddr_in));
	tcp_socket.sin_family = AF_INET;
	tcp_socket.sin_port = htons(port);

	/*Assigning the server address, in case a host name is given- search it at the NET*/
	if((tcp_socket.sin_addr.s_addr = inet_addr(server_addr)) == (unsigned long)INADDR_NONE) {
		host = gethostbyname(server_addr);
		if(host == (struct hostent *)NULL) {
			perror("HOST NOT FOUND");
			return -2;
		}
		memcpy(&tcp_socket.sin_addr, host->h_addr, sizeof(tcp_socket.sin_addr));
	}
	if((connect(sock,(struct sockaddr *)&tcp_socket, sizeof(tcp_socket)))< 0){
		perror("Client-connect() error");
		return -1;
	}
	else
		printf("Connection established...\n");
	return 2;
}

int send_hello(char* argv[]){

	int ans;
	uint8_t buff[3];
	client_command hello;

	hello.command_type=0;
	hello.data=0;
	printf("Data established..\n");

	buff[0]=hello.command_type;
	buff[1]=(uint8_t)( hello.data /256);
	buff[2]=((uint8_t)hello.data & 0X00FF);

	if((ans=send(sock, buff, 3, 0))==-1) {
		perror("send():");
		return 0;
	}
	printf("Hello Sent...\n");
	return 1;
}

int wait4welcome(){
	int rcv=1,welcome;
	fd_set set;
	struct timeval timeout;
	char buf[PACKET_SIZE];
	char maddr[50];

	bzero(buf, PACKET_SIZE);
	timeout.tv_sec=0;
	timeout.tv_usec = 500000;		//timeout is set to 0.5 sec
	FD_ZERO(&set);          /* initialize the fd set */
	FD_SET(sock, &set); /* add socket fd */
	FD_SET(0, &set);        /* add stdin fd (0) */

	printf("Starting select...\n");
	while(1){
		if ((rcv=select(sock+1, &set, 0, 0, &timeout)) < 0) {
			perror("ERROR in select");
			return -2;
		}
		if(!rcv){ //welcome timeout
			perror("Welcome timeout! Disconnecting...\n");
			return -2;
		}
		/* if the user has entered a command, process it */
		printf("no errors so far.\n");
		if (FD_ISSET(0, &set)) {
			fgets(buf,3, stdin);
			switch (buf[0]) {
			case 'q': /* terminate the server */
				printf("Closing Connection, Bye bye!\n");
				return -2;
				break;
			default: /* bad input */
				printf("ERROR: unknown command\n");
				printf("server> ");
				fflush(stdout);
			}
		}
		/* if a connection transmission has arrived, process it */
		if (FD_ISSET(sock, &set)) {//read: read input string from the client
			bzero(buf, PACKET_SIZE);
			if((welcome = read(sock, buf,PACKET_SIZE))<=0){
				perror("ERROR reading from socket");
				return -2;//disconnecting
			}
			//Check replyType error, or InvalidCommand option.
			if (welcome!=WELCOME_LEN || buf[0]!=0){ //cheking replyType!=0
				if(buf[0]==2){
					print_invalid_command(buf);
					return -2;
				}
				else{
					perror("ERROR: wrong replyType. Disconnecting...\n");
					return -2;
				}
			}
			//buf[9]='\0';
			//TODO: check if multicast and port is needed.
			numStations=((uint16_t)buf[1])*256+((uint16_t)buf[2]);
			printf("Number of Stations: %d\n",numStations);
			multicastGroup[3]=buf[3];
			multicastGroup[2]=buf[4];
			multicastGroup[1]=buf[5];
			multicastGroup[0]=buf[6];
			printf("Multicast:%d.%d.%d.%d\n",multicastGroup[0],multicastGroup[1],multicastGroup[2],multicastGroup[3]);
			printf("Received welcome\n");
			break;
		}
	}
	return 3;
}

int ask_song(){
	int rcv=1,i,ans,station_no=-1;
	uint8_t query[3];
	client_command Ask;
	fd_set set;
	char buf[PACKET_SIZE];

	bzero(buf, PACKET_SIZE);
	FD_ZERO(&set);          /* initialize the fd set */
	FD_SET(sock, &set); 	/* add socket fd */
	FD_SET(0, &set);        /* add stdin fd (0) */

	printf("Waiting for command...\n");
	if ((rcv=select(sock+1, &set, 0, 0, NULL)) < 0) {
		perror("ERROR in select");
		return -2;
	}

	/* if the user has entered a command, process it */
	if (FD_ISSET(0, &set)) {
		scanf("%s",buf);
		if(!isalpha(buf[0]))
			station_no=atoi(buf);
		if(buf[0]=='q'){
			printf("Quitting as you requested, my lord..\n");
			return -2;
		}

		if(station_no<numStations && station_no>=0){

			printf("Sending query about station no: %d\n",station_no);
			Ask.command_type=1;
			Ask.data=station_no;

			query[0]=Ask.command_type;
			query[1]=(uint8_t)( Ask.data /256);
			query[2]=((uint8_t)Ask.data & 0X00FF);

			if((ans=send(sock, query, 3, 0))==-1) {
				perror("send():");
				return 0;
			}
			printf("Ask Sent...\n");
			return 4;
		}
		printf("No such station.\nQuitting..\n");
		return -2;
	}
	/*if we received transmission we didn't expect*/
	printf("Error: Unknown/unexpected transmission arrived");
	return -2;
}//ask_song


int waitForAnnounce(){
	int rcv=1,i,announce,songNameSize;
	fd_set set;
	struct timeval timeout;
	char buf[PACKET_SIZE];

	bzero(buf, PACKET_SIZE);
	timeout.tv_sec=0;
	timeout.tv_usec = 500000;		//timeout is set to 0.5 sec
	FD_ZERO(&set);          /* initialize the fd set */
	FD_SET(sock, &set); /* add socket fd */
	FD_SET(0, &set);        /* add stdin fd (0) */

	printf("Starting select...\n");
	while(1){
		if ((rcv=select(sock+1, &set, 0, 0, &timeout)) < 0) {
			perror("in select");
			return -2;
		}
		if(!rcv){ //Announce timeout
			perror("Announce timeout! Disconnecting...\n");
			return -2;
		}
		/* if the user has entered a command, process it */
		printf("no errors so far.\n");
		if (FD_ISSET(0, &set)) {
			fgets(buf,3, stdin);
			switch (buf[0]) {
			case 'q': /* terminate the server */
				printf("Closing Connection, Bye bye!\n");
				return -2;
				break;
			default: /* bad input */
				printf("ERROR: unknown command\n");
				printf("server> ");
				fflush(stdout);
			}
		}
		/* if a connection transmission has arrived, process it */
		if (FD_ISSET(sock, &set)) {//read: read input string from the client
			bzero(buf, PACKET_SIZE);
			if((announce = read(sock, buf,PACKET_SIZE))<=0){
				perror("ERROR reading from socket");
				return -2;//disconnecting
			}

			//Check replyType error, or InvalidCommand option.
			if (buf[0]!=1){ //cheking replyType!=1-error
				if(buf[0]==2){
					print_invalid_command(buf);
					return -2;
				}
				else{
					perror("ERROR: wrong replyType. Disconnecting...\n");
					return -2;
				}
			}//right reply
			puts("Song name: ");
			songNameSize =  buf[1];
			for(i=2;i<(songNameSize+2);i++){
				printf("%c",buf[i]);
			}
			printf("\n");//YORED SHURA
			return 3;
		}

	}
}//waitForAnnounce


void print_invalid_command(char* buf){
	int i,replySize=buf[1];
	for(i=2;i<(replySize+2);i++){
		printf("%c",buf[i]);
	}
}//print_invalid_command
