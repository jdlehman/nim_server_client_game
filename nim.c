//Jonathan Lehman March 8, 2011

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


int MSG_LENGTH = 25;
int PASS_LENGTH = 20;
char handle[20];
char oppHandle[20];
char dgHost[30];
char dgPort[15];
int passMode = 0;
char* password = NULL;

int notOver = 1;
int notReceiveOnly = 1;
int lost = 0;
int resign = 0;
int dgSock = -1;

char* dgReceivePort = NULL;
char* dgSendPort = NULL;
char* listPortNum = NULL;
char* host = NULL;

//functions
void printConfig(int, int, int, int);
void printRow(int);
void doMove(int, int, int*, int*, int*, int*, char[]);
int moveConfig(int, int, int*, int*, int*, int*);
int charToInt(char);
int isLoss(int, int, int, int);

void sendDataGram(char, char[], int);
void receiveDataGram();
int setUpReceiveDataGram();
void getPortData();
void setUpHost();

void connectToNimServer();

void interpretMsg(char[]);
void initialServerMsg(char[]);
void wrongPassword();
void unexpectedMsg(char[]);
void wonGame();
void oppResign();
void oppMove(char[]);
void getOppHandle(char[]);
void oppQuit();
void nimServShutdown();
void serverFull();
void connectionCheck(char[]);

int readData(int, char[]);
int writeData(int, char[]);

int main(int argc, char *argv[])
{
	int queryMode = 0;

	//check that there are no more than 3 arguments not including file location
	if(argc > 4)
	{
		fprintf(stderr, "\nnim: Invalid arguments for nim. The correct invocation is:\nnim {-q} {-p string}\n");
		exit(1);
	}

	//check if arg1 exists
	if(argc > 1)
	{
		if((abs(strcmp(argv[1], "-q")) && abs(strcmp(argv[1], "-p"))))
		{
			fprintf(stderr, "\nnim: Invalid first argument for nim, '%s'. The first argument must be '-q' or '-p'.\n", argv[1]);
			exit(1);
		}
		else
		{
			//set to query mode or password mode
			if(abs(strcmp(argv[1], "-q")) == 0)
			{
				//arg = -q
				queryMode = 1;
			}
			else
			{
				//arg = -p
				passMode = 1;
			}
		}
	}

	//check if arg2 exists
	if(argc > 2)
	{
		if(queryMode)//-q was first argument
		{
			if(abs(strcmp(argv[2], "-p")))
			{
				fprintf(stderr, "\nnim: Invalid second argument for nim, '%s'. The second argument must be '-p'.\n", argv[2]);
				exit(1);
			}
			else
			{
				//arg = -p
				passMode = 1;
			}
		}
		else//-p was first argument
		{
			if((abs(strcmp(argv[2], "-q")) && abs(strcmp(argv[2], "-p"))))
			{
				//set password string (ending with null)
				if(strlen(argv[2]) > PASS_LENGTH)
				{
					fprintf(stderr, "Nim password should be no more than 20 characters.\n");
					exit(1);
				}
				else
				{
					password = malloc(sizeof(char) * (sizeof(argv[2]) + 1));
					password[sizeof(argv[2])] = '\0';
					strcpy(password, argv[2]);
				}
			}
			else
			{
				fprintf(stderr, "\nnim: Invalid second argument for nim, '%s'. The first argument was '-p' so the second argument must be a valid string that is not '-p' or '-q'.\n", argv[2]);
				exit(1);
			}
		}
	}
	else
	{
		if(passMode){
			fprintf(stderr, "\nnim: Need a second argument for nim because the first argument was '-p' so the second argument must be a valid string that is not '-p' or '-q'.\n");
			exit(1);
		}
	}

	//check if there is a third argument
	if(argc > 3)
	{
		if(queryMode && passMode)//argument must be valid string
		{
			if((abs(strcmp(argv[3], "-q")) && abs(strcmp(argv[3], "-p"))))
			{
				//set password string (ending with null)
				if(strlen(argv[3]) > PASS_LENGTH)
				{
					fprintf(stderr, "Nim password should be no more than 20 characters.\n");
					exit(1);
				}
				else
				{
					password = malloc(sizeof(char) * (sizeof(argv[3]) + 1));
					password[sizeof(argv[3])] = '\0';
					strcpy(password, argv[3]);
				}
			}
			else
			{
				fprintf(stderr, "\nnim: Invalid third argument for nim, '%s'. The second argument was '-p' so the third argument must be a valid string that is not '-p' or '-q'.\n", argv[3]);
				exit(1);
			}
		}
		else//argument must be -q
		{
			if(abs(strcmp(argv[3], "-q")))
			{
				fprintf(stderr, "\nnim: Invalid third argument for nim, '%s'. The third argument must be '-q'.\n", argv[3]);
				exit(1);
			}
			else
			{
				//arg = -q
				queryMode = 1;
			}
		}

	}
	else
	{
		//argument 3 must exist if queryMode and passMode are true
		if(queryMode && passMode)
		{
			fprintf(stderr, "\nnim: Need a third argument for nim because the second argument was '-p' so the third argument must be a valid string that is not '-p' or '-q'.\n");
			exit(1);
		}
	}

	getPortData();

	//send datagram to server if query mode (wait 60 seconds for reply then terminate with message)
	if(queryMode)
	{
		//send query to nim_server
		sendDataGram('q', password, passMode);
		//try to get datagram back from nim_server
		receiveDataGram();
	}
	else
	{
		connectToNimServer();
	}

	//free memory
	if(strlen(dgReceivePort) > 0)
	{
		free(dgReceivePort);
	}
	if(strlen(dgSendPort) > 0)
	{
		free(dgSendPort);
	}
	if(strlen(listPortNum) > 0)
	{
		free(listPortNum);
	}
	if(passMode){
		free(password);
	}

	return(0);

}

void getPortData()
{
	fprintf(stdout, "\nAttempting to locate server data.\n");
	FILE* file = NULL;
	if((file = fopen("nim_server_data.txt","r")) == NULL)
	{
		fprintf(stdout, "\nCould not locate the nim server data file.  The nim server might be down. Trying again in 60 seconds, please wait...\n");
		sleep(60);
		if((file = fopen("nim_server_data.txt","r")) == NULL)
		{
			fprintf(stdout, "\nStill could not locate the nim server data file. The nim server must be down.\n");
			exit(0);
		}
	}

	char buffer[30];
	while(fgets(buffer,30,file) != 0)
	{
		if(buffer[0] == 's')
		{
			dgSendPort = malloc(sizeof(char) * (strlen(buffer)));
			strncpy(dgSendPort, buffer+2, strlen(buffer) - 3);//-3 gets rid of \n
		}
		else if(buffer[0] == 'r')
		{
			dgReceivePort = malloc(sizeof(char) * (strlen(buffer)));
			strncpy(dgReceivePort, buffer+2, strlen(buffer) - 3);//-3 gets rid of \n
		}
		else if(buffer[0] == 'l')
		{
			listPortNum = malloc(sizeof(char) * (strlen(buffer)));
			strncpy(listPortNum, buffer+2, strlen(buffer) - 2);//-2 gets rid of \n
		}
		else if(buffer[0] == 'h')
		{
			host = malloc(sizeof(char) * (strlen(buffer)));
			strncpy(host, buffer+2, strlen(buffer) - 3);//-3 gets rid of \n
		}
	}

	if(fclose(file) != 0)
	{
		perror("nim:");
		exit(1);
	}

	fprintf(stdout, "\nNim server data located.\n");
}

int isLoss(int c1, int c2, int c3, int c4)
{
	return ((c1 == 0) && (c2 == 0) && (c3 == 0) && (c4 == 0));
}

void doMove(int o1, int o2, int *c1, int *c2, int *c3, int *c4, char msg[])
{
	if(o1 != -2 && o2 != -2)
	{
		fprintf(stdout, "\n%s made the move: '%d %d', which resulted in the following board configuration:\n", oppHandle, o1, o2);
	}
	else
	{
		fprintf(stdout, "\nCurrent board configuration:\n");
	}

	printConfig(*c1, *c2, *c3, *c4);

	int moveConfigVal;
	char tmp[10];
	int m1 = -1;
	int m2 = -1;
	fprintf(stdout, "\nPlease enter a valid move (row number then column number separated by space):  ");
	//read move
	if(!fgets(tmp, sizeof(tmp), stdin))
	{
		fprintf(stderr, "/nnim:fgets, error reading user move");
		exit(1);
	}

	tmp[strlen(tmp) - 1] = '\0';//get rid of new line character

	if(strlen(tmp) == 3 && ((m1 = charToInt(tmp[0])) > -1) && ((m2 = charToInt(tmp[2])) > -1) && ((moveConfigVal = moveConfig(m1, m2, c1, c2, c3, c4)) != -1))
	{
		//valid do nothing
	}
	else//invalid input
	{
		//ensure valid input from user
		do
		{
			do
			{
				fprintf(stdout, "\nInvalid move, %s. Please enter a valid move (row number then column number separated by a space, example: '3 4'):", tmp);
				//reread move
				memset(msg, 0, MSG_LENGTH);
				if(!fgets(tmp, sizeof(tmp), stdin))
				{
					fprintf(stderr, "/nnim:fgets, error reading user move");
					exit(1);
				}

				tmp[strlen(tmp) - 1] = '\0';//get rid of new line character

				if(strlen(tmp) == 3)
				{
					m1 = charToInt(tmp[0]);
					m2 = charToInt(tmp[2]);
				}
				else
				{
					m1 = -1;
					m2 = -1;
				}

			}while(m1 < 0 || m2 < 0);
		}while((moveConfigVal = moveConfig(m1, m2, c1, c2, c3, c4)) == -1);
	}

	if(moveConfigVal == 0)
	{
		//resign
		fprintf(stdout, "You have chosen to resign. Notifying '%s'.\n", oppHandle);
		memset(msg, 0, MSG_LENGTH);
		strcpy(msg, "r-");
		//cause nim to quit
		notOver = 0;
		resign = 1;
	}
	else
	{
		//validmove
		fprintf(stdout, "\nResult of your move: %s\n", tmp);
		printConfig(*c1, *c2, *c3, *c4);

		//check if user lost
		if(isLoss(*c1, *c2, *c3, *c4)){
			memset(msg, 0, MSG_LENGTH);
			strcpy(msg, "w-");
			fprintf(stdout, "\nYou took the last piece.  You lost the game. Notifying '%s' of victory.\n", oppHandle);
			//cause nim to quit after writing message to server
			notOver = 0;
			lost = 1;
		}
		else
		{
			//create message to send with move and config
			msg[0] = 'm';
			char moveAndConfig[9];
			sprintf(moveAndConfig, "%d%d,%d%d%d%d-", m1, m2, *c1, *c2, *c3, *c4);
			moveAndConfig[8] = '\0';
			strcat(msg, moveAndConfig);
			fprintf(stdout, "\nWaiting for '%s' to move...\n", oppHandle);
		}
	}
}

//returns 0 for exit, -1 for invalidMove, 1 for normal
int moveConfig(int m1, int m2, int *c1, int *c2, int *c3, int *c4)
{

	if((m1 < 0 || m2 < 0) || (m1 == 0 && m2 !=0) || (m2 == 0 && m1 != 0))
	{
		return -1;
	}
	if(m1 == 0 && m2 == 0)
	{
		return 0;
	}

	switch(m1)
	{
		case 1:
			if(*c1 >= m2)
			{
				*c1 = m2 - 1;
			}
			else
			{
				return -1;
			}
			break;
		case 2:
			if(*c2 >= m2)
			{
				*c2 = m2 - 1;
			}
			else
			{
				return -1;
			}
			break;
		case 3:
			if(*c3 >= m2)
			{
				*c3 = m2 - 1;
			}
			else
			{
				return -1;
			}
			break;
		case 4:
			if(*c4 >= m2)
			{
				*c4 = m2 - 1;
			}
			else
			{
				return -1;
			}
			break;
		default :
			return -1;
			break;
	}

	return 1;
}

void printConfig(int r1, int r2, int r3, int r4)
{
	fprintf(stdout, "\nYour handle: '%s'\n", handle);
	fprintf(stdout, "Opponent's handle: '%s'\n", oppHandle);

	fprintf(stdout, "\nBoard:\n");
	fprintf(stdout, "1| ");
	//print row1
	printRow(r1);
	fprintf(stdout, "\n2| ");
	//print row2
	printRow(r2);
	fprintf(stdout, "\n3| ");
	//print row3
	printRow(r3);
	fprintf(stdout, "\n4| ");
	//print row4
	printRow(r4);
	fprintf(stdout, "\n +------------------------- ");
	fprintf(stdout, "\n   1 2 3 4 5 6 7\n");
}

void printRow(int x)
{
	int i;
	for(i = 0; i < x; i++)
	{
		fprintf(stdout, "0 ");
	}
}

void setUpHost()
{
	//modified from http://www.unix.com/programming/5919-get-ip-address.html
	char buf[200] ;
	struct hostent *Host = (struct hostent*)malloc(sizeof(struct hostent));
	gethostname (buf, 200);
	Host = (struct hostent*) gethostbyname(buf);

	memset(dgHost, 0, 30);
	sprintf(dgHost, "%s", inet_ntoa(*((struct in_addr *)Host->h_addr)));
	dgHost[strlen(dgHost)] = '\0';
}

void sendDataGram(char mtype, char mpass[], int passMode)
{
	int socket_fd, cc, ecode;
	struct sockaddr_in *dest;
	struct addrinfo hints, *addrlist;
	struct {char msgType; char hasPass; char pass[20]; char nimHost[30]; char dgPortNum[15];} msgbuf;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	setUpHost();
	dgSock = setUpReceiveDataGram();
	ecode = getaddrinfo(host, dgSendPort, &hints, &addrlist);
	if (ecode != 0)
	{
		fprintf(stderr, "nim,sendDataGram: getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}
	dest = (struct sockaddr_in *) addrlist->ai_addr;

	socket_fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd == -1)
	{
		perror ("nim,sendDataGram:socket");
		exit (1);
	}

	msgbuf.msgType = mtype;
	strcpy(msgbuf.nimHost, dgHost);
	strcpy(msgbuf.dgPortNum, dgPort);

	//set password if exists
	if(passMode)
	{
		strcpy(msgbuf.pass, mpass);
		msgbuf.hasPass = 'y';
	}
	else
	{
		msgbuf.hasPass = 'n';
	}

	cc = sendto(socket_fd, &msgbuf, sizeof(msgbuf), 0, (struct sockaddr *) dest, sizeof(struct sockaddr_in));
	if (cc < 0)
	{
		perror("nim,sendDataGram:sendto");
		exit(1);
	}
}

int setUpReceiveDataGram()
{
	int socket_fd, ecode;
	socklen_t length;
	struct sockaddr_in *s_in;
	struct addrinfo hints, *addrlist;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
	hints.ai_protocol = 0; hints.ai_canonname = NULL;
	hints.ai_addr = NULL; hints.ai_next = NULL;
	ecode = getaddrinfo(NULL, "0", &hints, &addrlist);
	if (ecode != 0)
	{
		fprintf(stderr, "nim: getaddrinfo: %s\n", gai_strerror(ecode));
		exit(1);
	}

	s_in = (struct sockaddr_in *) addrlist->ai_addr;

	socket_fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd < 0)
	{
		perror ("nim: socket");
		exit(1);
	}

	 if (bind(socket_fd, (struct sockaddr *)s_in, sizeof(struct sockaddr_in)) < 0)
	 {
		perror("nim:bind");
		exit(1);
	 }

	 /*
	get port number assigned to this process by bind().
	*/
	length = sizeof(struct sockaddr_in);
	if (getsockname(socket_fd, (struct sockaddr *)s_in, &length) < 0) {
		perror("nim:getsockname");
		exit(1);
	}

	memset(dgPort, 0, 15);
	sprintf(dgPort, "%d", ntohs(s_in->sin_port));
	dgPort[strlen(dgPort)] = '\0';

	return socket_fd;
}

void receiveDataGram()
{
	int cc, hits;
	fd_set mask;
	struct timeval timeout;
	socklen_t fsize;
	struct sockaddr_in from;
	struct {char msgType; char h1g1[21]; char h2g1[21]; char h1g2[21]; char h2g2[21]; char handleWaiting[21];} msg;

	for(;;)
	{
		fsize = sizeof(from);

		FD_ZERO(&mask);
		FD_SET(dgSock,&mask);
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;

		fprintf(stdout, "\nWaiting for a reply from nim_server...\n");
		if ((hits = select(dgSock+1, &mask, (fd_set *)0, (fd_set *)0, &timeout)) < 0)
		{
		  perror("nim,receiveDataGram:select");
		  exit(1);
		}
		if ( (hits==0) || ((hits>0) && (FD_ISSET(0,&mask))) ) {
		  fprintf(stdout, "No reply from nim_server in 60 seconds.\nShutting down..\nPlease try querying again.\n");
		  exit(0);
		}

		cc = recvfrom(dgSock, &msg, sizeof(msg), 0, (struct sockaddr *) &from, &fsize);

		if(cc < 0)
		{
			perror("nim,receiveDataGram:recvfrom");
			exit(1);
		}

		int dataWritten = 0;
		fprintf(stdout, "\n");

		//check msg type
		if(msg.msgType == '0')
		{
			//write if users are waiting to play, or if games are in progress
			if(strlen(msg.handleWaiting) > 0)
			{
				dataWritten = 1;
				fprintf(stdout, "User, '%s', is waiting to play nim.\n", msg.handleWaiting);
			}
			if(strlen(msg.h1g1) > 0)
			{
				dataWritten = 1;
				fprintf(stdout, "Game of nim is in progress between '%s' and '%s'.\n", msg.h1g1, msg.h2g1);
			}
			if(strlen(msg.h1g2) > 0)
			{
				dataWritten = 1;
				fprintf(stdout, "Game of nim is in progress between '%s' and '%s'.\n", msg.h1g2, msg.h2g2);
			}
			//write message if no users waiting to play or currently playing on nim server
			if(!dataWritten)
			{
				fprintf(stdout, "There are currently no users waiting to play nim and no games in progress.\n");
			}
		}
		else if(msg.msgType == '1')
		{
			if(password != NULL)
			{
				fprintf(stdout, "Incorrect password, '%s', for nim server. Request could not be processed.\n", password);
			}
			else
			{
				fprintf(stdout, "A password is required for nim server. Request could not be processed.\n");
			}
		}
		else if(msg.msgType == '2')
		{
			fprintf(stdout, "Password not needed for nim server. Try requesting data without password field ('nim -q')\n");
		}

		exit(0);

	}
}

void connectToNimServer()
{
	int sock, ecode;
	struct sockaddr_in *server;
	struct addrinfo hints, *addrlist;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(host, listPortNum, &hints, &addrlist);
	if (ecode != 0) {
	  fprintf(stderr, "nim: getaddrinfo: %s\n", gai_strerror(ecode));
	  exit(1);
	}

	server = (struct sockaddr_in *) addrlist->ai_addr;

	/*
	   Create the socket.
	*/
	if ((sock = socket( addrlist->ai_family, addrlist->ai_socktype, 0 )) < 0 ) {
		perror("nim:socket");
		exit(1);
	}

	/*
	   Connect to data socket on the peer at the specified Internet
	   address.
	 */
	if(connect(sock, (struct sockaddr *)server, sizeof(struct sockaddr_in)) < 0) {
			perror("nim:connect");
			exit(1);
	}

	char msg[MSG_LENGTH];
	strcpy(msg, "p");
	if(passMode)
	{
		strcat(msg, password);
	}
	strcat(msg, "-");

	//write message
	if(writeData(sock, msg) < 1)
	{
		notOver = 0;
	}

	while(notOver)
	{
		if(readData(sock, msg) > 0)
		{
			interpretMsg(msg);
		}
		else
		{
			notOver = 0;
		}

		if(notReceiveOnly)
		{
			//write message
			if(writeData(sock, msg) < 1)
			{
				notOver = 0;
			}
		}

		//reset
		notReceiveOnly = 1;
	}

	//quit nim when done
	if(!lost && !resign)
	{
		fprintf(stdout, "\nServer unexpectedly shut down. Sorry for the inconvenience.");
	}
	fprintf(stdout, "\nQuitting game.\n");
	exit(0);
}

void interpretMsg(char msg[])
{
	switch(msg[0])
	{
	    case 'i':
	        initialServerMsg(msg);
	        break;
	    case 'e' :
	    	wrongPassword();
	        break;
	    case 'm' :
	    	oppMove(msg);
	    	break;
	    case 'h' :
			getOppHandle(msg);
			break;
	    case 'r' :
			oppResign();
			break;
	    case 'w' :
			wonGame();
			break;
	    case 'q' :
			oppQuit();
			break;
	    case 's' :
			nimServShutdown();
			break;
	    case 'f' :
			serverFull();
			break;
	    case 'c' :
			connectionCheck(msg);
			break;
	    default :
	        unexpectedMsg(msg);
	        break;
	}
}

void connectionCheck(char msg[])
{
	//nim_server was just checking that client did not quit,
	memset(msg, 0, MSG_LENGTH);
	strcpy(msg, "y-");
}

void serverFull()
{
	fprintf(stdout, "\nThe nim server is at capacity. Sorry for the inconvenience, please try connecting later.\n");
	exit(0);
}

void nimServShutdown()
{
	fprintf(stdout, "\nThe nim server is shutting down. Sorry for the inconvenience.");
	fprintf(stdout, "\nQuitting game.\n");
	exit(0);
}

void oppQuit()
{
	fprintf(stdout, "\nOpponent has unexpectedly quit.");
	fprintf(stdout, "\nQuitting game.\n");
	exit(0);
}

void wonGame()
{
	fprintf(stdout, "\nOpponent, '%s', has taken the last piece on the board. You win. The final board configuration is as follows:\n", oppHandle);
	printConfig(0, 0, 0, 0);
	fprintf(stdout, "\nQuitting game.\n");
	exit(0);
}

void oppResign()
{
	fprintf(stdout, "\nOpponent, '%s', has resigned. You win.", oppHandle);
	fprintf(stdout, "\nQuitting game.\n");
	exit(0);
}

void oppMove(char msg[])
{
	msg++;//get rid of first char, m
	char* msgPtr = strtok(msg, ",");
	char oMove[3];
	strncpy(oMove, msgPtr, sizeof(oMove));
	oMove[2] = '\0';

	//next token
	msgPtr = strtok(NULL, ",");
	char boardConfig[5];
	strncpy(boardConfig, msgPtr, sizeof(boardConfig));
	boardConfig[4] = '\0';

	//clear message
	memset(msg, 0, MSG_LENGTH);
	msg--;

	int c1;
	int c2;
	int c3;
	int c4;
	c1 = charToInt(boardConfig[0]);
	c2 = charToInt(boardConfig[1]);
	c3 = charToInt(boardConfig[2]);
	c4 = charToInt(boardConfig[3]);

	//print what opponent did and prompt user to move
	doMove(charToInt(oMove[0]), charToInt(oMove[1]), &c1, &c2, &c3, &c4, msg);
}

void getOppHandle(char msg[])
{
	msg++;//get rid of first char, h
	strncpy(oppHandle, msg, strlen(msg));
	notReceiveOnly = 0;
	fprintf(stdout, "\nServer found an available player to play nim. Starting match with player: '%s'\n", oppHandle);
	msg--;
}

void initialServerMsg(char msg[])
{
	//clear msg out
	memset(msg, 0, MSG_LENGTH);
	msg[0] = 'h';
	char tmp[60];
	fprintf(stdout, "\nConnection with nim_server established. Please enter a handle (no spaces and more than 20 characters).\n");
	if(!fgets(tmp, 60, stdin))
	{
		fprintf(stderr, "/nnim:fgets, error reading user handle");
		exit(1);
	}

	//make sure user enters input
	while(strlen(tmp) < 2)
	{
		fprintf(stdout, "\nInvalid entry. Please enter a handle (no more than 20 characters).\n");
		memset(tmp, 0, 60);
		if(!fgets(tmp, 22, stdin))
		{
			fprintf(stderr, "/nnim:fgets, error reading user handle");
			exit(1);
		}
	}

	tmp[strlen(tmp) - 1] = '\0';//get rid of new line character

	if(strlen(tmp) > 20)
	{
		strncat(handle, tmp, 20);
		strncat(msg, handle, 20);
	}
	else
	{
		strncat(handle, tmp, strlen(tmp));
		strncat(msg, handle, strlen(handle));
	}
	strcat(msg, "-");

	fprintf(stdout, "\nWaiting to be matched up with another player...\n");

	//clear out stdin
	rewind(stdin);
	fflush(stdin);
}

void unexpectedMsg(char msg[])
{
	fprintf(stderr, "\nAn unexpected message, '%s', was received from the server. Ending connection.\n", msg);
	exit(1);
}

void wrongPassword()
{
	if(password != NULL)
	{
		fprintf(stdout, "\nPassword, '%s', was incorrect. Please try again.\n", password);
	}
	else
	{
		fprintf(stdout, "\nA password is required to connect to the nim server. Request could not be processed. Please try again.\n");
	}

	exit(0);
}

//converts chars 0-7 to ints otherwise return -1 (error)
int charToInt(char ch)
{
	int rtn = -1;
	switch(ch)
	{
		case '0':
			rtn = 0;
			break;
		case '1':
			rtn = 1;
			break;
		case '2':
			rtn = 2;
			break;
		case '3':
			rtn = 3;
			break;
		case '4':
			rtn = 4;
			break;
		case '5':
			rtn = 5;
			break;
		case '6':
			rtn = 6;
			break;
		case '7':
			rtn = 7;
			break;
		case '9':
			rtn = -2;
			break;
		default:
			rtn = -1;
			break;
	}

	return rtn;
}

//return >0 if read data
int readData(int sock, char msg[])
{
	memset(msg, 0, MSG_LENGTH);

	char ch = ' ';
	int ctr = 0;
	while(read(sock, &ch, 1) == 1 && (ch != '-') && (ctr < MSG_LENGTH))
	{
		msg[ctr] = ch;
		ctr++;
	}

	//fprintf(stdout, "nim %d read %s\n", sock, msg);

	return ctr;
}

//return 1 if wrote data
int writeData(int sock, char msg[])
{
	int left, num, put;
	int wroteSomething = 0;
	left = strlen(msg); put=0;
	while (left > 0){
		if((num = write(sock, msg+put, left)) < 0) {
			perror("nim:write");
			exit(1);
		}
		else
		{
			wroteSomething = 1;
			left -= num;
		}
		put += num;
	}

	//fprintf(stdout, "nim %d wrote %s\n", sock, msg);

	memset(msg, 0, MSG_LENGTH);

	return wroteSomething;
}
