/******************************************************************************
* tcp_client.c
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include "networks.h"

#define INIT_INPUT_LEN 3
#define MAXBUF 1400
#define DEBUG_FLAG 1
#define xstr(a) str(a)
#define str(a) #a

void sendToServer(int socketNum);
void checkArgs(int argc, char * argv[]);

void selectHandlerLoop(int clientSocket, u_char *handle);
void clientPrompt(int clientSocket, u_char *handle);
void sendB(int clientSocket, char *buf, u_char *handle);
void sendE(int clientSocket);
void sendL(int clientSocket);
void sendM(int clientSocket, char *buf, u_char *handle);


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor

	// check args
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	u_char *handle = (u_char *) argv[1];

	sendFlag01(socketNum, handle);

	selectHandlerLoop(socketNum, handle);

	close(socketNum);

	return 0;
}

void selectHandlerLoop(int clientSocket, u_char *handle) {
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(clientSocket, &fdset);
	FD_SET(fileno(stdin), &fdset);

	if(select(clientSocket + 1, &fdset, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "select error\n");
	}
	if(FD_ISSET(clientSocket, &fdset)) {
		recv2(clientSocket, NULL, fdset, handle);
	}

	while(1){
		FD_ZERO(&fdset);
		FD_SET(clientSocket, &fdset);
		FD_SET(fileno(stdin), &fdset);

		printf ("$: ");
		fflush(stdout);
		if(select(clientSocket + 1, &fdset, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "select error\n");
		}
		if(FD_ISSET(clientSocket, &fdset)) {
			printf("\n");
			recv2(clientSocket, NULL, fdset, handle);
		} else {
			clientPrompt(clientSocket, handle);
		}

	}
}

void clientPrompt(int clientSocket, u_char *handle) {
	char buf[MAXBUF];
	memset(buf, 0, MAXBUF);
	scanf ("%[^\n]s", buf);
	if(strlen(buf) > 1400) {
		fprintf(stderr, "Packet too big (>1400 bytes)\n");
		while ( getchar() != '\n' );
		fflush(stdout);
		fflush(stdin);
		printf("hi\n");
		return;
	}
	switch (tolower(buf[1])) {
		case 'm':
			sendM(clientSocket, buf, handle);
			break;
		case 'b':
			sendB(clientSocket, buf, handle);
			break;
		case 'l':
			sendL(clientSocket);
			break;
		case 'e':
			sendE(clientSocket);
			break;
		default:
			printf("Invalid command\n");
	}
	while ( getchar() != '\n' );
	fflush(stdout);
	fflush(stdin);
	return;


}

void sendE(int clientSocket) {
	sendFlag08(clientSocket);
}

void sendL(int clientSocket) {
	sendFlag10(clientSocket);
}

void sendM(int clientSocket, char *buf, u_char *handle) {
	u_char *handles[100];
	int numHandles, distanceToMessage = INIT_INPUT_LEN;
	u_char *message;
	char bufCpy[MAXBUF];
	memset(bufCpy, 0, MAXBUF);
	memcpy(bufCpy, buf, strlen(buf));
	char *delimBuf = strtok(bufCpy, " ");
	delimBuf = strtok(NULL, " ");
	if(!delimBuf) {
		fprintf(stderr, "Usage: %m [# of handles] handle1 [handle2] ... message\n");
		return;
	}
	if(!(numHandles = strtol(delimBuf, NULL, 10))) {
		*handles = (u_char*) delimBuf;
		distanceToMessage += strlen(delimBuf) + 1 /* for space after handle */;
		message = (u_char*) buf + distanceToMessage;
		sendFlag05(clientSocket, handle, 1, handles, message);
		return;
	}
	if(numHandles > 9) {
		fprintf(stderr, "Too many handles used (>9)\n");
		return;
	}


	delimBuf = strtok(NULL, " ");
	distanceToMessage+=2;
	int i;
	for(i = 0; i < numHandles; i++) {
		if(!delimBuf) {
			printf("here4\n");
			fprintf(stderr, "Usage: %m [# of handles] handle1 [handle2] ... message\n");
			return;
		}
		*(handles + i) = (u_char*) delimBuf;
		distanceToMessage += strlen(delimBuf) + 1 /* for space after handle */;
		delimBuf = strtok(NULL, " ");
	}
	message = (u_char*) buf + distanceToMessage;

	sendFlag05(clientSocket, handle, numHandles, handles, message);
}

void sendB(int clientSocket, char *buf, u_char *handle) {
	int distanceToMessage = INIT_INPUT_LEN;
	u_char *message;

	message = (u_char*) buf + distanceToMessage;

	sendFlag04(clientSocket, handle, message);
}


void sendToServer(int socketNum)
{
	char sendBuf[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */

	printf("Enter the data to send: ");
	scanf("%" xstr(MAXBUF) "[^\n]%*[^\n]", sendBuf);

	sendLen = strlen(sendBuf) + 1;
	printf("read: %s len: %d\n", sendBuf, sendLen);

	sent =  send(socketNum, sendBuf, sendLen, 0);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("String sent: %s \n", sendBuf);
	printf("Amount of data sent is: %d\n", sent);
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		fprintf(stderr, "usage: %s handle host-name port-number \n", argv[0]);
		exit(1);
	}

	if(strlen(argv[1]) > 100) {
		fprintf(stderr, "Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(1);
	}

	if(strtol(argv[1], NULL, 10) != 0 || argv[1][0] == '0') {
		fprintf(stderr, "Invalid handle, handle starts with a number\n");
		exit(1);
	}
}
