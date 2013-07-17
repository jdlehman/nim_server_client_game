//Jonathan Lehman March 8, 2011

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/wait.h>

//declare functions
void sendDataGram(int);
void receiveDataGram(int, char[]);
//void acceptConnection();
void writeMsg(int, char[]);
void readMsg();
void interpretMsg(char[]);
void getHandle(char[]);
void checkPass(char[]);

void connectAndListen();
int setUpListener();
void writePortData(char[]);
int setUpReceiveDataGram();
int setUpSendDataGram();
void setUpHost();

int readData(int, char[]);
int writeData(int, char[]);
void deleteNimServFile();
int checkIfNimServRunning();

void usr2handler();
void sigchildhandler();

int PASS_LENGTH = 20;
int conns[2];
int connCtr = 0;
int gameNum = 0;
int currGame = 0;
int game1PID = -1;
int game2PID = -1;

int listSock = -1;
int dgRSock = -1;
int dgSSock = -1;
int waiting = 0;
int correctPass = 0;
int MSG_LENGTH = 25;
int MAX_GAMES = 2;
char handleWaiting[20];
char handleWaiting2[20];
char handle1Game1[20];
char handle2Game1[20];
char handle1Game2[20];
char handle2Game2[20];
char nimDGHost[30];
char nimDGPort[15];
char* password = NULL;
int passMode = 0;

void usr2handler()
{
	//delete nim server file
	deleteNimServFile();
	//send signal to all group process (nim_match_servers) to close down cleanly
	if(game1PID != -1)
	{
		kill(game1PID, SIGUSR1);
	}
	if(game2PID != -1)
	{
		kill(game2PID, SIGUSR1);
	}
	exit(1);
}


void sigchildhandler()
{
	pid_t pid;

	pid = wait(NULL);

	//check which child game it exited
	if(pid == game1PID)
	{
		gameNum--;
		game1PID = -1;
		//clear arrays
		memset(handle1Game1, 0, 20);
		memset(handle2Game1, 0, 20);
	}
	else if(pid == game2PID)
	{
		gameNum--;
		game2PID = -1;
		//clear arrays
		memset(handle1Game2, 0, 20);
		memset(handle2Game2, 0, 20);
	}
}

int main(int argc, char *argv[])
{

	//prelim mem clear
	memset(handleWaiting, 0, 20);
	memset(handleWaiting2, 0, 20);
	memset(handle1Game1, 0, 20);
	memset(handle2Game1, 0, 20);
	memset(handle1Game2, 0, 20);
	memset(handle2Game2, 0, 20);

	//check if nim_server is already running
	if(checkIfNimServRunning())
	{
		fprintf(stdout, "\nThe nim server is already running.\n");
		exit(0);
	}
	//attach handler to method to handle signals
	signal(SIGUSR2, usr2handler);
	signal(SIGCHLD, sigchildhandler);

	//check that there is no more than 1 argument not including file location
	if(argc > 2)
	{
		fprintf(stderr, "\nnim_server: Invalid arguments for nim_server. The correct invocation is:\nnim_server {string}\n");
		exit(1);
	}

	//check if arg1 exists
	if(argc > 1)
	{
		passMode = 1;
		//set password string (ending with null)
		if(strlen(argv[1]) > PASS_LENGTH)
		{
			fprintf(stderr, "Nim Server password must be less than 20 characters.\n");
			exit(1);
		}
		else
		{
			password = malloc(sizeof(char) * (sizeof(argv[1]) + 1));
			password[sizeof(argv[1])] = '\0';
			strcpy(password, argv[1]);
		}
	}

	setUpHost();
	dgRSock = setUpReceiveDataGram();
	//dgSSock = setUpSendDataGram();

	listSock = setUpListener();
	connectAndListen();

	if(passMode)
	{
		free(password);
	}

	deleteNimServFile();

	return 0;
}

void setUpHost()
{
	//modified from http://www.unix.com/programming/5919-get-ip-address.html
	char buf[200] ;
	struct hostent *Host = (struct hostent*)malloc(sizeof(struct hostent));
	gethostname (buf, 200);
	Host = (struct hostent*) gethostbyname(buf);

	char tmp[20];
	sprintf(tmp, "h=%s\n", inet_ntoa(*((struct in_addr *)Host->h_addr)));
	tmp[strlen(tmp)] = '\0';

	writePortData(tmp);
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
		fprintf(stderr, "nim_server: getaddrinfo: %s\n", gai_strerror(ecode));
		raise(SIGUSR2);
	}

	s_in = (struct sockaddr_in *) addrlist->ai_addr;

	socket_fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd < 0)
	{
		perror ("nim_server: socket");
		raise(SIGUSR2);
	}

	 if (bind(socket_fd, (struct sockaddr *)s_in, sizeof(struct sockaddr_in)) < 0)
	 {
		perror("nim_server:bind");
		raise(SIGUSR2);
	 }

	 /*
	get port number assigned to this process by bind().
	*/
	length = sizeof(struct sockaddr_in);
	if (getsockname(socket_fd, (struct sockaddr *)s_in, &length) < 0) {
		perror("nim_server:getsockname");
		raise(SIGUSR2);
	}

	char tmp[20];
	sprintf(tmp, "s=%d\n", ntohs(s_in->sin_port));
	tmp[strlen(tmp)] = '\0';


	writePortData(tmp);

	return socket_fd;
}

int setUpSendDataGram()
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
		fprintf(stderr, "nim_server: getaddrinfo: %s\n", gai_strerror(ecode));
		raise(SIGUSR2);
	}

	s_in = (struct sockaddr_in *) addrlist->ai_addr;

	socket_fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd < 0)
	{
		perror ("nim_server: socket");
		raise(SIGUSR2);
	}

	 if (bind(socket_fd, (struct sockaddr *)s_in, sizeof(struct sockaddr_in)) < 0)
	 {
		perror("nim_server:bind");
		raise(SIGUSR2);
	 }

	 /*
	get port number assigned to this process by bind().
	*/
	length = sizeof(struct sockaddr_in);
	if (getsockname(socket_fd, (struct sockaddr *)s_in, &length) < 0) {
		perror("nim_server:getsockname");
		raise(SIGUSR2);
	}

	char tmp[20];
	sprintf(tmp, "r=%d\n", ntohs(s_in->sin_port));
	tmp[strlen(tmp)] = '\0';

	writePortData(tmp);

	return socket_fd;
}

int setUpListener()
{
	int listener;  /* fd for socket on which we get connection requests */
	struct sockaddr_in *localaddr;
	int ecode;
	socklen_t length;
	struct addrinfo hints, *addrlist;

	/*
	Want to specify local server address of:
	  addressing family: AF_INET
	  ip address:        any interface on this system
	  port:              0 => system will pick free port
	*/

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(NULL, "0", &hints, &addrlist);
	if (ecode != 0) {
		fprintf(stderr, "nim_server: getaddrinfo: %s\n", gai_strerror(ecode));
		raise(SIGUSR2);
	}

	localaddr = (struct sockaddr_in *) addrlist->ai_addr;

	/*
	Create socket on which we will accept connections. This is NOT the
	same as the socket on which we pass data.
	*/
	if ( (listener = socket( addrlist->ai_family, addrlist->ai_socktype, 0 )) < 0 ) {
		perror("nim_server:socket");
		raise(SIGUSR2);
	}


	//find available port to use
	if (bind(listener, (struct sockaddr *)localaddr, sizeof(struct sockaddr_in)) < 0) {
		perror("nim_server:bind");
		raise(SIGUSR2);
	}

	/*
	get port number assigned to this process by bind().
	*/
	length = sizeof(struct sockaddr_in);
	if (getsockname(listener, (struct sockaddr *)localaddr, &length) < 0) {
		perror("nim_server:getsockname");
		raise(SIGUSR2);
	}

	char tmp[20];
	sprintf(tmp, "l=%d", ntohs(localaddr->sin_port));
	tmp[strlen(tmp)] = '\0';

	writePortData(tmp);

	return listener;
}

void receiveDataGram(int passMode, char pass[])
{
	int cc;
	struct sockaddr from;
	socklen_t fsize = sizeof(from);
	struct{char msgType; char hasPass; char pass[20]; char nimHost[30]; char dgPortNum[15];} msg;

	cc = recvfrom(dgRSock, &msg, sizeof(msg), 0, (struct sockaddr *) &from, &fsize);

	if(cc < 0)
	{
		perror("nim_server:recvfrom");
		raise(SIGUSR2);
	}

	strcpy(nimDGHost, msg.nimHost);
	strcpy(nimDGPort, msg.dgPortNum);

	//fill with info about games in progress or users waiting to play (if password right)
	if(passMode)
	{
		if((msg.msgType == 'q') && (msg.hasPass == 'y') && (strcmp(msg.pass, pass) == 0))
		{
			sendDataGram(0);
		}
		else//tell nim that pass is wrong
		{
			sendDataGram(1);
		}
	}
	else
	{
		if((msg.msgType == 'q') && (msg.hasPass == 'n'))
		{
			sendDataGram(0);
		}
		else//tell nim pass is not needed
		{
			sendDataGram(2);
		}
	}
}

void sendDataGram(int sendType)
{
	int socket_fd, cc, ecode;
	struct sockaddr_in *dest;
	struct addrinfo hints, *addrlist;
	struct {char msgType; char h1g1[21]; char h2g1[21]; char h1g2[21]; char h2g2[21]; char handleWaiting[21];} msgbuf;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV; hints.ai_protocol = 0;
	hints.ai_canonname = NULL; hints.ai_addr = NULL;
	hints.ai_next = NULL;

	ecode = getaddrinfo(nimDGHost, nimDGPort, &hints, &addrlist);
	//sleep(.001);//TODO
	//fprintf(stdout, "server nimDGHost %s, nimDGPort %s\n", nimDGHost, nimDGPort);
	if (ecode != 0)
	{
		fprintf(stderr, "nim_server: getaddrinfo: %s\n", gai_strerror(ecode));
		raise(SIGUSR2);
	}
	dest = (struct sockaddr_in *) addrlist->ai_addr;

	socket_fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0);
	if (socket_fd == -1)
	{
		perror ("nim_server:socket");
		raise(SIGUSR2);
	}

	if(sendType == 0)//pass correct
	{
		memset(msgbuf.handleWaiting, 0, 21);
		memset(msgbuf.h1g1, 0, 21);
		memset(msgbuf.h2g1, 0, 21);
		memset(msgbuf.h1g2, 0, 21);
		memset(msgbuf.h2g2, 0, 21);

		msgbuf.msgType = '0';
		strncpy(msgbuf.handleWaiting, handleWaiting, strlen(handleWaiting));
		strcat(msgbuf.handleWaiting, "\0");
		strncpy(msgbuf.h1g1, handle1Game1, strlen(handle1Game1));
		strcat(msgbuf.h1g1, "\0");
		strncpy(msgbuf.h2g1, handle2Game1, strlen(handle2Game1));
		strcat(msgbuf.h2g1, "\0");
		strncpy(msgbuf.h1g2, handle1Game2, strlen(handle1Game2));
		strcat(msgbuf.h1g2, "\0");
		strncpy(msgbuf.h2g2, handle2Game2, strlen(handle2Game2));
		strcat(msgbuf.h2g2, "\0");
	}
	else if(sendType == 1)//pass incorrect
	{
		msgbuf.msgType = '1';
	}
	else if(sendType == 2)//no pass needed
	{
		msgbuf.msgType = '2';
	}

	cc = sendto(socket_fd, &msgbuf, sizeof(msgbuf), 0, (struct sockaddr *) dest, sizeof(struct sockaddr_in));
	if (cc < 0)
	{
		perror("nim_server:sendto");
		raise(SIGUSR2);
	}

	close(socket_fd);
}


void connectAndListen()
{
	int hits;
	struct sockaddr_in peer, from;
	socklen_t length, fsize;
	char msg[MSG_LENGTH];
	fd_set mask;
	struct timeval timeout;

	/*
	Now accept a single connection. Upon connection, data will be
	passed through the socket on descriptor conn.
	*/
	listen(listSock, 1);

	while(1){
		while(waiting < 2)
		{

			fsize = sizeof(from);

			FD_ZERO(&mask);
			FD_SET(dgRSock,&mask);
			FD_SET(listSock,&mask);
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;

			//select statement
			if ((hits = select(listSock+1, &mask, (fd_set *)0, (fd_set *)0, &timeout)) < 0)
			{
//			  perror("nim_server:select");//TODO
//			  raise(SIGUSR2);
			}

			if (((hits > 0) && (FD_ISSET(dgRSock,&mask))))
			{
				//check if waiting user has quit
				if(waiting == 1)
				{
					int failed = 0;
					char msg2[MSG_LENGTH];

					strcpy(msg2, "c-");

					if(writeData(conns[connCtr - 1], msg2) != 1)
					{
						failed = 1;
					}

					if(readData(conns[connCtr - 1], msg2) == 0)
					{
						failed = 1;
					}

					//check if socket is open
					if(failed)
					{
						close(conns[connCtr - 1]);
						connCtr--;
						waiting--;
						memset(handleWaiting, 0, 20);
					}
				}

				//reply to datagram
				receiveDataGram(passMode, password);
			}
			else if(((hits > 0) && (FD_ISSET(listSock,&mask))) )
			{
				//check if waiting user has quit
				if(waiting == 1)
				{
					int failed = 0;
					char msg2[MSG_LENGTH];

					strcpy(msg2, "c-");

					if(writeData(conns[connCtr - 1], msg2) != 1)
					{
						failed = 1;
					}

					if(readData(conns[connCtr - 1], msg2) == 0)
					{
						failed = 1;
					}

					//check if socket is open
					if(failed)
					{
						close(conns[connCtr - 1]);
						connCtr--;
						waiting--;
						memset(handleWaiting, 0, 20);
					}
				}

				length = sizeof(peer);

				if ((conns[connCtr]=accept(listSock, (struct sockaddr *)&peer, &length)) < 0) {
					perror("nim_server:accept");
					raise(SIGUSR2);
				}

				//read from nim
				readData(conns[connCtr], msg);

				interpretMsg(msg);

				if(correctPass)
				{
					strcpy(msg, "i-");
				}
				else
				{
					strcpy(msg, "e-");
				}

				//check if have capacity for additional game
				if(gameNum == MAX_GAMES)
				{
					//send message to client telling them server is full
					memset(msg, 0, MSG_LENGTH);
					strcpy(msg, "f-");

					//write message
					if(writeData(conns[connCtr], msg) != 1)
					{
						//didnt write data
						close(conns[connCtr]);
					}
				}
				else//server can handle additional game, accept client
				{
					//write initial message
					if(writeData(conns[connCtr], msg) != 1)
					{
						//didnt write data
						close(conns[connCtr]);
					}

					if(readData(conns[connCtr], msg) == 0)
					{
						//didnt read data from client
						close(conns[connCtr]);
					}
					else//read data from client
					{
						interpretMsg(msg);

						if(correctPass)
						{
							waiting++;
							connCtr++;
						}
					}

					//reset
					correctPass = 0;
				}
			}
		}

		//reset match
		waiting = 0;
		connCtr--;

		//copy data from waiting to in game
		if(strlen(handle1Game1) > 0)
		{
			strncpy(handle1Game2, handleWaiting, strlen(handleWaiting));
			strncpy(handle2Game2, handleWaiting2, strlen(handleWaiting2));
			currGame = 2;
		}
		else
		{
			strncpy(handle1Game1, handleWaiting, strlen(handleWaiting));
			strncpy(handle2Game1, handleWaiting2, strlen(handleWaiting2));
			currGame = 1;
		}

		//clear msg
		memset(msg, 0, MSG_LENGTH);

		//write opponent handle to each client
		msg[0] = 'h';
		strcat(msg, handleWaiting2);
		strcat(msg, "-");

		int createMatchServ = 1;

		if(writeData(conns[connCtr - 1], msg) != 1)
		{
			createMatchServ = 0;
			//tell other play that opponent quit
			msg[0] = 'q';
			strcat(msg, "-");

			close(conns[connCtr - 1]);
			writeData(conns[connCtr], msg);

			if(currGame == 1)
			{
				//clear arrays
				memset(handle1Game1, 0, 20);
				memset(handle2Game1, 0, 20);
			}
			else
			{
				//clear arrays
				memset(handle1Game2, 0, 20);
				memset(handle2Game2, 0, 20);
			}
		}
		else
		{
			msg[0] = 'h';
			strcat(msg, handleWaiting);
			strcat(msg, "-");

			if(writeData(conns[connCtr], msg) != 1)
			{
				createMatchServ = 0;
				//tell other play that opponent quit
				msg[0] = 'q';
				strcat(msg, "-");

				writeData(conns[connCtr - 1], msg);
				close(conns[connCtr]);

				if(currGame == 1)
				{
					//clear arrays
					memset(handle1Game1, 0, 20);
					memset(handle2Game1, 0, 20);
				}
				else
				{
					//clear arrays
					memset(handle1Game2, 0, 20);
					memset(handle2Game2, 0, 20);
				}
			}
		}

		//clear arrays
		memset(handleWaiting, 0, 20);
		memset(handleWaiting2, 0, 20);


		if(createMatchServ)
		{
			int status;

			if(vfork() == 0)
			{
				close(listSock);
				if(currGame == 1)
				{
					game1PID = getpid();
				}
				else
				{
					game2PID = getpid();
				}

				char* arg1;
				char* arg2;
				//convert to string
				sprintf(arg1=malloc(sizeof(conns[connCtr - 1])), "%d", conns[connCtr - 1]);
				sprintf(arg2=malloc(sizeof(conns[connCtr])), "%d", conns[connCtr]);

				execl("nim_match_server", "nim_match_server", arg1, arg2, NULL);
			}
			else
			{
				close(conns[connCtr - 1]);
				close(conns[connCtr]);

				//increment number of games running
				gameNum++;

				//wait for child to change state
				while(wait3(&status, WNOHANG, NULL));
			}
		}

		//reset connCtr
		connCtr = 0;
	}
}

void writePortData(char data[])
{
	FILE* file = NULL;
	if((file = fopen("nim_server_data.txt","a+")) == NULL)
	{
		perror("nim_server:");
		exit(1);
	}

	fprintf(file, "%s", data);

	if(fclose(file) != 0)
	{
		perror("nim_server:");
		exit(1);
	}
}

void deleteNimServFile()
{
	unlink("nim_server_data.txt");
}

int checkIfNimServRunning()
{
	FILE* file = NULL;
	if((file = fopen("nim_server_data.txt","r")) == NULL)
	{
		return 0;
	}

	if(fclose(file) != 0)
	{
		perror("nim_server:");
		exit(1);
	}

	return 1;
}

//interpret msg
void interpretMsg(char msg[])
{
	switch(msg[0])
	{
	    case 'h':
	        getHandle(msg);
	        break;
	    case 'p' :
	    	checkPass(msg);
	        break;
	    default :
	    	memset(msg, 0, MSG_LENGTH);
	    	strcpy(msg, "-");
	    	break;
	}
}

void getHandle(char msg[])
{
	if(strlen(handleWaiting) > 0)
	{
		msg++;
		strncpy(handleWaiting2, msg, strlen(msg));
		msg--;
	}
	else
	{
		msg++;
		strncpy(handleWaiting, msg, strlen(msg));
		msg--;
	}
}

void checkPass(char msg[])
{
	if(passMode)
	{
		msg++;
		if(strcmp(msg, password) == 0)
		{
			correctPass = 1;
		}
		else
		{
			correctPass = 0;
		}
		msg--;
	}
	else
	{
		correctPass = 1;
	}
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
			perror("nim_server:write");
			exit(1);
		}
		else
		{
			wroteSomething = 1;
			left -= num;
		}
		put += num;
	}

	memset(msg, 0, MSG_LENGTH);

	return wroteSomething;
}
