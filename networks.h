
/* 	Code originally give to Prof. Smith by his TA in 1994.
	No idea who wrote it.  Copy and use at your own Risk
*/


#ifndef __NETWORKS_H__
#define __NETWORKS_H__

#define BACKLOG 10

typedef struct handle {
   u_char handleName[100];
   int socket;
} handle;

typedef struct handleList {
   handle *currHandle;
   struct handleList *prevHandle;
	struct handleList *nextHandle;
} handleList;


//in server.c
handleList* removeClientFromList(handleList *myHandleList);

// for the server side
void printHandleList(handleList* myHandleList);
int tcpServerSetup(int portNumber);
int tcpAccept(int server_socket, int debugFlag);
int checkHandleInList(u_char* handle, handleList* myHandleList);
handleList* addHandleToTable(u_char* handle, int handleLen, handleList* myHandleList);

//duplicated
handleList* recv2(int clientSocket, handleList *myHandleList, fd_set fdset, u_char *handle);

// for the client side
int tcpClientSetup(char * serverName, char * port, int debugFlag);

//for both sides
handleList* parsePacket(u_char *buf, handleList *myHandleList, int clientSocket, fd_set fdset, u_char *handle);
u_char* sendFlag01(int socketNum, u_char *handle);
void sendFlag02(int socketNum);
void sendFlag03(int socketNum);
void sendFlag04(int clientSocket, u_char *senderHandle, u_char *message);
void forwardFlag04(u_char* buf, u_char *senderHandle, handleList *myHandleList);
void sendFlag05(int clientSocket, u_char *senderHandle, int numHandles, u_char **handles, u_char *message);
void forwardFlag05(u_char* buf, u_char *senderHandle, u_char *destHandle, handleList *myHandleList);
void sendFlag07(u_char* buf, u_char *senderHandle, u_char *destHandle, handleList *myHandleList);
void sendFlag08(int socketNum);
void sendFlag09(int socketNum);
void sendFlag10(int socketNum);
void sendFlag11(int socketNum, handleList *myHandleList);
void sendFlag12(int socketNum, handleList *myHandleList);
handleList* recvFlag01(u_char *buf, handleList *myHandleList);
int recvFlag02(u_char *buf);
int recvFlag03(u_char *buf, u_char *handle);
int recvFlag04Client(u_char *buf);
int recvFlag04Server(u_char *buf, handleList *myHandleList);
int recvFlag05Client(u_char *buf);
int recvFlag05Server(u_char *buf, handleList *myHandleList);
int recvFlag07(u_char *buf);
handleList* recvFlag08(u_char *buf, int clientSocket, handleList* myHandleList);
int recvFlag09(u_char *buf, int clientSocket);
void recvFlag10(u_char *buf, handleList *myHandleList);
int recvFlag11(u_char *buf);
void recvFlag12(u_char *buf);
int getSocketFromHandle(u_char *destHandle, handleList *myHandleList);
u_long getNumHandles(handleList *myHandleList);

#endif
