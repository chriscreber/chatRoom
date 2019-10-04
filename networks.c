
// Hugh Smith April 2017
// Network code to support TCP client server connections

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

#include "networks.h"
#include "gethostbyname6.h"

#define MAXBUF 1400
#define HEADER_LEN 3

/* This function sets the server socket.  It lets the system
determine the port number.  The function returns the server
socket number and prints the port number to the screen.  */

int tcpServerSetup(int portNumber)
{
	int server_socket= 0;
	struct sockaddr_in6 server;      /* socket address for local side  */
	socklen_t len= sizeof(server);  /* length of local address        */

	/* create the tcp socket  */
	server_socket= socket(AF_INET6, SOCK_STREAM, 0);
	if(server_socket < 0)
	{
		perror("socket call");
		exit(1);
	}

	server.sin6_family= AF_INET6;
	server.sin6_addr = in6addr_any;   //wild card machine address
	server.sin6_port= htons(portNumber);

	/* bind the name (address) to a port */
	if (bind(server_socket, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		perror("bind call");
		exit(-1);
	}

	//get the port name and print it out
	if (getsockname(server_socket, (struct sockaddr*)&server, &len) < 0)
	{
		perror("getsockname call");
		exit(-1);
	}

	if (listen(server_socket, BACKLOG) < 0)
	{
		perror("listen call");
		exit(-1);
	}

	printf("Server is using port %d \n", ntohs(server.sin6_port));

	return server_socket;
}

// This function waits for a client to ask for services.  It returns
// the client socket number.

int tcpAccept(int server_socket, int debugFlag)
{
	struct sockaddr_in6 clientInfo;
	int clientInfoSize = sizeof(clientInfo);
	int client_socket= 0;

	if ((client_socket = accept(server_socket, (struct sockaddr*) &clientInfo, (socklen_t *) &clientInfoSize)) < 0)
	{
		perror("Server Terminated");
		exit(-1);
	}

	if (debugFlag)
	{
		// printf("Client accepted.  Client IP: %s Client Port Number: %d\n",
		// 		getIPAddressString(clientInfo.sin6_addr.s6_addr), ntohs(clientInfo.sin6_port));
	}


	return(client_socket);
}

// recv function for server and client
handleList* recv2(int clientSocket, handleList *myHandleList, fd_set fdset, u_char *handle)
{
	u_char buf[MAXBUF];
	memset(buf, 0, MAXBUF);
	u_char *bufPtr = buf;
	int messageLen = 0;
	int pduLen;

	// get the header
	if ((messageLen = recv(clientSocket, bufPtr, 2, MSG_WAITALL)) < 0)
	{
		perror("recv call");
		exit(-1);
	} else if(messageLen == 0) {
		return removeClientFromList(myHandleList);
	}

	memcpy(&pduLen, bufPtr, 2);
	bufPtr += 2;

	//now get the data from the client_socket
	if ((messageLen = recv(clientSocket, bufPtr, ntohs(pduLen) - 2, MSG_WAITALL)) < 0)
	{
		perror("recv call");
		exit(-1);
	}


	return parsePacket(buf, myHandleList, clientSocket, fdset, handle);

}

int tcpClientSetup(char * serverName, char * port, int debugFlag)
{
	// This is used by the client to connect to a server using TCP

	int socket_num;
	uint8_t * ipAddress = NULL;
	struct sockaddr_in6 server;

	// create the socket
	if ((socket_num = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	{
		perror("socket call");
		exit(-1);
	}

	// setup the server structure
	server.sin6_family = AF_INET6;
	server.sin6_port = htons(atoi(port));

	// get the address of the server
	if ((ipAddress = getIPAddress6(serverName, &server)) == NULL)
	{
		exit(-1);
	}

	if(connect(socket_num, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect call");
		exit(-1);
	}

	if (debugFlag)
	{
		// printf("Connected to %s IP: %s Port Number: %d\n", serverName, getIPAddressString(ipAddress), atoi(port));
	}

	return socket_num;
}

handleList* parsePacket(u_char *buf, handleList *myHandleList, int clientSocket, fd_set fdset, u_char *handle) {
	u_char flag;
	int numHandle = 0;
	int i = 0;
	memcpy(&flag, buf+2, 1);
	switch (flag) {
		case 1:
			myHandleList = recvFlag01(buf, myHandleList);
			break;
		case 2:
			recvFlag02(buf);
			break;
		case 3:
			recvFlag03(buf, handle);
			break;
		case 4:
			if(myHandleList == NULL) {
				recvFlag04Client(buf);
			} else {
				recvFlag04Server(buf, myHandleList);
			}
			break;
		case 5:
			if(myHandleList == NULL) {
				recvFlag05Client(buf);
			} else {
				recvFlag05Server(buf, myHandleList);
			}
			break;
		case 7:
			recvFlag07(buf);
			break;
		case 8:
			myHandleList = recvFlag08(buf, clientSocket, myHandleList);
			break;
		case 9:
			recvFlag09(buf, clientSocket);
			break;
		case 10:
			recvFlag10(buf, myHandleList);
			break;
		case 11:
			numHandle = recvFlag11(buf);
			for(i = 0; i < numHandle; i++) {
				if(select(clientSocket + 1, &fdset, NULL, NULL, NULL) < 0) {
					fprintf(stderr, "select error\n");
				}
				if(FD_ISSET(clientSocket, &fdset)) {
					recv2(clientSocket, NULL, fdset, handle);
				}
			}
			break;
		case 12:
			recvFlag12(buf);
			break;
		default:
			fprintf(stderr, "Packet not implemented\n");
	}
	return myHandleList;
}

// flag packet receivers

handleList* recvFlag01(u_char *buf, handleList *myHandleList) {
	u_char handle[100];
	int handleLen = buf[3];
	char newLine = 0;
	memset(handle, 0, 100);
	memcpy(handle, buf+4, handleLen);
	memcpy(handle+handleLen, &newLine, 1);
	handleLen += 1;
	if(checkHandleInList(handle, myHandleList)) {
		// handle already exists
		sendFlag03(myHandleList->currHandle->socket);
		myHandleList = removeClientFromList(myHandleList);

	} else {
		// handle does not exist
		myHandleList = addHandleToTable(handle, handleLen, myHandleList);
		sendFlag02(myHandleList->currHandle->socket);
	}
	return myHandleList;
}

int checkHandleInList(u_char* handle, handleList* myHandleList) {
	while(myHandleList != NULL) {
		if(strcmp((char *) myHandleList->currHandle->handleName, (char *) handle) == 0) {
			return 1;
		}
		myHandleList = myHandleList->prevHandle;
	}
	return 0;
}

handleList* addHandleToTable(u_char* handle, int handleLen, handleList* myHandleList) {
	memcpy(myHandleList->currHandle->handleName, handle, handleLen);
	return myHandleList;
}

int recvFlag02(u_char *buf) {
	return 0;
}

int recvFlag03(u_char *buf, u_char *handle) {
	printf("Handle already in use: %s\n", handle);
	close(0);
	exit(1);
	return 0;
}

int recvFlag04Client(u_char *buf) {
	u_int pduLen;
	u_char senderHandleLen;
	u_char senderHandle[100];
	memset(senderHandle, 0, 100);

	memcpy(&pduLen, buf, 2);
	pduLen = htons(pduLen);
	buf += 3;
	memcpy(&senderHandleLen, buf, 1);
	buf += 1;
	memcpy(&senderHandle, buf, senderHandleLen);
	buf += senderHandleLen;

	printf("%s: %s\n", senderHandle, buf);

	return 0;
}

int recvFlag04Server(u_char *buf, handleList *myHandleList) {
	u_int pduLen;
	u_char senderHandleLen;
	u_char *baseBuf = buf;
	u_char senderHandle[100];
	memset(senderHandle, 0, 100);

	memcpy(&pduLen, buf, 2);
	pduLen = ntohs(pduLen);
	buf += 3;
	memcpy(&senderHandleLen, buf, 1);
	buf += 1;
	memcpy(&senderHandle, buf, senderHandleLen);
	buf += senderHandleLen;

	forwardFlag04(baseBuf, senderHandle, myHandleList);

	return 0;
}

int recvFlag05Client(u_char *buf) {
	u_int pduLen;
	u_char senderHandleLen, destHandleLen;
	int numHandles;
	u_char senderHandle[100];
	memset(senderHandle, 0, 100);

	memcpy(&pduLen, buf, 2);
	pduLen = htons(pduLen);
	buf += 3;
	memcpy(&senderHandleLen, buf, 1);
	buf += 1;
	memcpy(&senderHandle, buf, senderHandleLen);
	buf += senderHandleLen;
	memcpy(&numHandles, buf, 1);
	buf += 1;

	int i;
	for(i = 0; i < numHandles; i++) {
		memcpy(&destHandleLen, buf, 1);
		buf += 1;
		buf += destHandleLen;
	}
	printf("%s: %s\n", senderHandle, buf);

	return 0;
}

int recvFlag05Server(u_char *buf, handleList *myHandleList) {
	u_int pduLen;
	u_char senderHandleLen, destHandleLen;
	u_char *baseBuf = buf;
	u_char destHandle[100], senderHandle[100];
	u_char numHandles;

	memset(destHandle, 0, 100);
	memset(senderHandle, 0, 100);

	memcpy(&pduLen, buf, 2);
	pduLen = ntohs(pduLen);
	buf += 3;
	memcpy(&senderHandleLen, buf, 1);
	buf += 1;
	memcpy(&senderHandle, buf, senderHandleLen);
	buf += senderHandleLen;
	memcpy(&numHandles, buf, 1);
	buf += 1;

	int i;
	for(i = 0; i < numHandles; i++) {
		memcpy(&destHandleLen, buf, 1);
		buf += 1;
		memset(destHandle, 0, 100);
		memcpy(&destHandle, buf, destHandleLen);
		buf += destHandleLen;
		forwardFlag05(baseBuf, senderHandle, destHandle, myHandleList);
	}

	return 0;
}

int recvFlag07(u_char *buf) {
	u_int pduLen;
	u_char senderHandleLen;
	u_char senderHandle[100];
	memset(senderHandle, 0, 100);

	memcpy(&pduLen, buf, 2);
	pduLen = htons(pduLen);
	buf += 3;
	memcpy(&senderHandleLen, buf, 1);
	buf += 1;
	memcpy(&senderHandle, buf, senderHandleLen);
	buf += senderHandleLen;

	fprintf(stderr, "Client with handle %s does not exist.\n", senderHandle);

	return 0;
}

handleList* recvFlag08(u_char *buf, int clientSocket, handleList* myHandleList) {
	sendFlag09(clientSocket);
	close(clientSocket);
	myHandleList = removeClientFromList(myHandleList);

	return myHandleList;
}

int recvFlag09(u_char *buf, int clientSocket) {
	close(clientSocket);
	exit(1);
	return clientSocket;
}

void recvFlag10(u_char *buf, handleList *myHandleList) {
	sendFlag11(myHandleList->currHandle->socket, myHandleList);
	sendFlag12(myHandleList->currHandle->socket, myHandleList);
}

int recvFlag11(u_char *buf) {
	u_long numHandles;

	memcpy(&numHandles, buf+3, 4);
	numHandles = ntohl(numHandles);
	printf("Number of clients: %lu\n", numHandles);

	return numHandles;
}

void recvFlag12(u_char *buf) {
	u_char handleName[100];
	memset(handleName, 0, 100);
	u_char handleLen;

	memcpy(&handleLen, buf+3, 1);
	memcpy(&handleName, buf+4, handleLen);
	printf("\t%s\n", handleName);

}

// flag packet senders

u_char* sendFlag01(int socketNum, u_char *handle) {
	u_short pduLen;
	u_char flag = 0x1;
	u_char handleLen = strlen((char*)handle);
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN + 1 + handleLen;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);
	memcpy(sendBuf+3, &handleLen, 1);
	memcpy(sendBuf+4, handle, handleLen);

	sent =  send(socketNum, sendBuf, htons(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	return handle;
}

void sendFlag02(int socketNum) {
	u_short pduLen;
	u_char flag = 0x2;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);

	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag03(int socketNum) {
	u_int pduLen;
	u_char flag = 0x3;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);

	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag04(int clientSocket, u_char *senderHandle, u_char *message) {
	u_char flag = 0x4;
	u_char handleLen = strlen((char*)senderHandle);
	int messageLen = strlen((char*) message);
	u_int pduLen = HEADER_LEN + 1 + handleLen;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);
	u_char *sendBufPtr = sendBuf;
	u_char newLine = 0;
	int i;

	sendBufPtr += 2;
	memcpy(sendBufPtr, &flag, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, &handleLen, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, senderHandle, handleLen);
	sendBufPtr += handleLen;

	for(i = 0; i < messageLen / 200; i++) {
		memcpy(sendBufPtr, message + i * 200, 200);
		pduLen += 200;
		sendBufPtr += 200;
		// put null at end of string
		memcpy(sendBufPtr, &newLine, 1);

		pduLen += 1;
		pduLen = htons(pduLen);
		memcpy(sendBuf, &pduLen, 2);

		sent = send(clientSocket, sendBuf, ntohs(pduLen), 0);

		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}

		pduLen = ntohs(pduLen);
		pduLen -= 201;
		sendBufPtr -= 200;
	}

	memcpy(sendBufPtr, message + (i * 200) , messageLen % 200);
	sendBufPtr += messageLen % 200;
	pduLen += messageLen % 200;
	// put null at end of string
	memcpy(sendBufPtr, &newLine, 1);
	pduLen += 1;
	pduLen = htons(pduLen);
	memcpy(sendBuf, &pduLen, 2);

	sent = send(clientSocket, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void forwardFlag04(u_char* buf, u_char *senderHandle, handleList *myHandleList) {
	int sent = 0;

	int pduLen;
	memcpy(&pduLen, buf, 2);
	pduLen = ntohs(pduLen);

	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}

	while(myHandleList != NULL) {
		if(strcmp((char*)senderHandle, (char*)myHandleList->currHandle->handleName)) {
			sent =  send(myHandleList->currHandle->socket, buf, pduLen, 0);

			if (sent < 0)
			{
				perror("send call");
				exit(-1);
			}
		}

		myHandleList = myHandleList->nextHandle;
	}

}



void sendFlag05(int clientSocket, u_char *senderHandle, int numHandles, u_char **handles, u_char *message) {
	u_char flag = 0x5;
	u_char handleLen = strlen((char*)senderHandle);
	int messageLen = strlen((char*) message);
	u_int pduLen = HEADER_LEN + 1 + handleLen + 1 + (1 * numHandles);
	u_char ucharNumHandles = (u_char) numHandles;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);
	u_char *sendBufPtr = sendBuf;
	u_char newLine = 0;


	sendBufPtr += 2;
	memcpy(sendBufPtr, &flag, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, &handleLen, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, senderHandle, handleLen);
	sendBufPtr += handleLen;
	memcpy(sendBufPtr, &ucharNumHandles, 1);
	sendBufPtr += 1;

	int i;
	for(i = 0; i < numHandles; i++) {
		handleLen = strlen((char*)*(handles + i));
		memcpy(sendBufPtr, &handleLen, 1);
		sendBufPtr += 1;
		memcpy(sendBufPtr, *(handles + i), handleLen);
		pduLen += handleLen;
		sendBufPtr += handleLen;
	}

	for(i = 0; i < messageLen / 200; i++) {
		memcpy(sendBufPtr, message + i * 200, 200);
		pduLen += 200;
		sendBufPtr += 200;
		// put null at end of string
		memcpy(sendBufPtr, &newLine, 1);

		pduLen += 1;
		pduLen = htons(pduLen);
		memcpy(sendBuf, &pduLen, 2);

		sent = send(clientSocket, sendBuf, ntohs(pduLen), 0);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}

		pduLen = ntohs(pduLen);
		pduLen -= 201;
		sendBufPtr -= 200;
	}

	memcpy(sendBufPtr, message + (i * 200) , messageLen % 200);
	sendBufPtr += messageLen % 200;
	pduLen += messageLen % 200;
	// put null at end of string
	memcpy(sendBufPtr, &newLine, 1);
	pduLen += 1;
	pduLen = htons(pduLen);
	memcpy(sendBuf, &pduLen, 2);

	sent = send(clientSocket, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}


}

void forwardFlag05(u_char* buf, u_char *senderHandle, u_char *destHandle, handleList *myHandleList) {
	int sent = 0;
	int clientSocket = getSocketFromHandle(destHandle, myHandleList);
	if(clientSocket == -1) {
		sendFlag07(buf, senderHandle, destHandle, myHandleList);
		return;
	}

	int pduLen;
	memcpy(&pduLen, buf, 2);
	pduLen = ntohs(pduLen);

	sent =  send(clientSocket, buf, pduLen, 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

int getSocketFromHandle(u_char *destHandle, handleList *myHandleList) {
	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}

	while(myHandleList != NULL) {
		if(strcmp((char*)destHandle, (char*)myHandleList->currHandle->handleName) == 0) {
			return myHandleList->currHandle->socket;
		}
		myHandleList = myHandleList->nextHandle;
	}
	return -1;
}

void sendFlag07(u_char* buf, u_char *senderHandle, u_char *destHandle, handleList *myHandleList) {
	u_char flag = 0x7;
	u_char handleLen = strlen((char*)destHandle);

	u_int pduLen = HEADER_LEN + 1 + handleLen;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);
	u_char *sendBufPtr = sendBuf;

	pduLen = htons(pduLen);
	memcpy(sendBuf, &pduLen, 2);
	sendBufPtr += 2;
	memcpy(sendBufPtr, &flag, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, &handleLen, 1);
	sendBufPtr += 1;
	memcpy(sendBufPtr, destHandle, handleLen);
	sendBufPtr += handleLen;

	int clientSocket = getSocketFromHandle(senderHandle, myHandleList);

	sent =  send(clientSocket, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag08(int socketNum) {
	u_int pduLen;
	u_char flag = 0x8;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);

	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag09(int socketNum) {
	u_int pduLen;
	u_char flag = 0x9;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);

	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag10(int socketNum) {
	u_short pduLen;
	u_char flag = 0xA;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	pduLen = HEADER_LEN;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);

	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag11(int socketNum, handleList *myHandleList) {
	u_short pduLen;
	u_char flag = 0xB;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);
	u_long numHandles = htonl(getNumHandles(myHandleList));

	pduLen = HEADER_LEN + 4 /* 4 bytes for # of handles */;
	pduLen = htons(pduLen);

	memcpy(sendBuf, &pduLen, 2);
	memcpy(sendBuf+2, &flag, 1);
	memcpy(sendBuf+3, &numHandles, 4);


	sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);

	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

void sendFlag12(int socketNum, handleList *myHandleList) {
	u_short pduLen;
	u_char flag = 0xC;
	int sent = 0;
	u_char sendBuf[MAXBUF];
	memset(sendBuf, 0, MAXBUF);

	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}
	while(myHandleList != NULL) {
		u_char handleLen = strlen((char*)myHandleList->currHandle->handleName);
		pduLen = HEADER_LEN + 1 + handleLen;
		pduLen = htons(pduLen);
		memcpy(sendBuf, &pduLen, 2);
		memcpy(sendBuf+2, &flag, 1);
		memcpy(sendBuf+3, &handleLen, 1);
		memcpy(sendBuf+4, myHandleList->currHandle->handleName, handleLen);

		sent =  send(socketNum, sendBuf, ntohs(pduLen), 0);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}
		myHandleList = myHandleList->nextHandle;
	}

}

u_long getNumHandles(handleList *myHandleList) {
	int numHandles = 0;
	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}
	while(myHandleList != NULL) {
		numHandles++;
		myHandleList = myHandleList->nextHandle;
	}
	return numHandles;
}

handleList* removeClientFromList(handleList *myHandleList) {

	int clientSocket = clientSocket = myHandleList->currHandle->socket;

	if(myHandleList->prevHandle == NULL) {
		if(myHandleList->nextHandle == NULL) {
			free(myHandleList->currHandle);
			free(myHandleList);
			close(clientSocket);
			return NULL;
		} else {
			myHandleList = myHandleList->nextHandle;
			free(myHandleList->prevHandle->currHandle);
			free(myHandleList->prevHandle);
			myHandleList->prevHandle = NULL;
			close(clientSocket);
			return myHandleList;
		}
	}

	if(myHandleList->nextHandle == NULL) {
		myHandleList = myHandleList->prevHandle;
		free(myHandleList->nextHandle->currHandle);
		free(myHandleList->nextHandle);
		myHandleList->nextHandle = NULL;
		close(clientSocket);
		return myHandleList;
	}

	handleList *tmpHandleList = myHandleList;

	myHandleList->prevHandle->nextHandle = myHandleList->nextHandle;
	myHandleList->nextHandle->prevHandle = myHandleList->prevHandle;


	free(tmpHandleList->currHandle);
	free(tmpHandleList);
	close(clientSocket);

	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}

	return myHandleList;

}

// Testing purposes
void printHandleList(handleList* myHandleList) {
	if(myHandleList == NULL) {
		printf("NULL handle list\n");
		return;
	}
	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}
	while(myHandleList != NULL) {
		if(myHandleList->currHandle != NULL) {
			if (myHandleList->currHandle->handleName == NULL) {
				printf("NULL currHandle->handleName\n");
			} else {
				printf("handleName: %s\n", myHandleList->currHandle->handleName);
			}
			if (myHandleList->currHandle->socket == 0) {
				printf("NULL currHandle->socket\n");
			} else {
				printf("socket: %u\n", myHandleList->currHandle->socket);
			}
		} else {
			printf("NULL currHandle\n");
		}
		myHandleList = myHandleList->nextHandle;

	}
}
