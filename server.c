/******************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
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

#include "networks.h"

#define MAXBUF 1400
#define DEBUG_FLAG 1

int checkArgs(int argc, char *argv[]);

handleList* addClientToList(int clientSocket, handleList *myHandleList);
int findMaxSocket(handleList *myHandleList);
void selectHandlerLoop(int serverSocket);

int main(int argc, char *argv[])
{
	int serverSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);

	//create the server socket
	serverSocket = tcpServerSetup(portNumber);

	// Main Handler for most of server
	selectHandlerLoop(serverSocket);

	return 0;
}

void selectHandlerLoop(int serverSocket) {
	int clientSocket = 0;   //socket descriptor for the client socket
	handleList *baseHandleList = NULL;
	handleList *myHandleList = baseHandleList;

	fd_set fdset;
	int maxSocket = serverSocket;
	while(1){
		FD_ZERO(&fdset);
		FD_SET(serverSocket, &fdset);
		//add child sockets to set
		while(myHandleList != NULL) {
			if(myHandleList->currHandle != NULL) FD_SET(myHandleList->currHandle->socket, &fdset);
			myHandleList = myHandleList->nextHandle;
		}
		myHandleList = baseHandleList;
		if(select(maxSocket + 1, &fdset, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "select error\n");
		}
		if(FD_ISSET(serverSocket, &fdset)) {
			clientSocket = tcpAccept(serverSocket, DEBUG_FLAG);
			baseHandleList = addClientToList(clientSocket, baseHandleList);
		}
		myHandleList = baseHandleList;

		while(myHandleList != NULL) {
			//go through sockets
			if(FD_ISSET(myHandleList->currHandle->socket, &fdset)){
				//read if needed
				myHandleList = recv2(myHandleList->currHandle->socket, myHandleList, fdset, NULL);
			}
			if(myHandleList == NULL) {
				baseHandleList = NULL;
			} else {
				myHandleList = myHandleList->nextHandle;
			}
		}

		myHandleList = baseHandleList;



		if(findMaxSocket(myHandleList) > maxSocket) {
			maxSocket = findMaxSocket(myHandleList);
		}
		myHandleList = baseHandleList;

	}
}

handleList* addClientToList(int clientSocket, handleList *myHandleList) {
	if(myHandleList == NULL) {
		myHandleList = (handleList*)malloc(sizeof(handleList));
	}

	if(myHandleList->currHandle == NULL) {
		myHandleList->currHandle = (handle*)malloc(sizeof(handle));
		memset(myHandleList->currHandle->handleName, 0, 100);
		myHandleList->currHandle->socket = clientSocket;
		return myHandleList;
	}

	while(myHandleList->nextHandle != NULL) {
		myHandleList = myHandleList->nextHandle;
	}

	myHandleList->nextHandle = (handleList*)malloc(sizeof(handleList));
	myHandleList->nextHandle->prevHandle = myHandleList;
	myHandleList = myHandleList->nextHandle;
	myHandleList->currHandle = (handle*)malloc(sizeof(handle));
	myHandleList->currHandle->socket = clientSocket;
	myHandleList->nextHandle = NULL; // added for regrade
	while(myHandleList->prevHandle != NULL) {
		myHandleList = myHandleList->prevHandle;
	}
	return myHandleList;
}



int findMaxSocket(handleList *myHandleList) {
	int maxSocket = 0;
	while(myHandleList != NULL) {
		if(myHandleList->currHandle == NULL) {
			return maxSocket;
		} else if(myHandleList->currHandle->socket > maxSocket) {
			maxSocket = myHandleList->currHandle->socket;
		}
		myHandleList = myHandleList->nextHandle;
	}
	return maxSocket;
}


int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}

	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}

	return portNumber;
}
