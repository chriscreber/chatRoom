Christopher Creber
ccreber@calpoly.edu
TCP Socket Chat Room

All of the code provided is mine besides the code provided by my professor in the gethostbyname6.* files

To use:
Server:  ./server [portNumber]
Example: ./server 2000

Client: ./cclient handle host-name port-number
%e - exits
%l - lists the handles of the other clinets
%b message - broadcasts the message to all other clients
%m [# of clients (not necessary if sending to only one client)] handle1 [handle2 ...] message - sends message to the clients with matching handles
Examples: %m user1 hello there
          %m 2 user1 user2 hey guys user3 is pretty lame right? 
