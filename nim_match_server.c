//Jonathan Lehman March 8, 2011

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>

void usr1handler();
int writeData(int, char[]);
int readData(int, char[]);

int MSG_LENGTH = 25;
int conn = -1;
int conn2 = -1;
int stillPlaying = 1;

void usr1handler()
{
	//tell other play that opponent quit
	char msg[MSG_LENGTH];
	memset(msg, 0, MSG_LENGTH);

	//tell users that nim_server is shutting down
	msg[0] = 's';
	strcat(msg, "-");
	writeData(conn, msg);
	writeData(conn2, msg);

	exit(1);
}

int main(int argc, char *argv[])
{

	//attach handler to method to handle signal
	signal(SIGUSR1, usr1handler);

	//check arguments needs at least 2
	if(argc != 3)
	{
		fprintf(stderr, "\nnim_match_server: Invalid arguments for nim_match_server.\n");
		exit(1);
	}

	char *invalChar1;long arg1;char *invalChar2;long arg2;

	//perform conversion
	//and check that there is no overflow
	if(((arg1 = strtol(argv[1], &invalChar1, 10)) >= INT_MAX) || ((arg2 = strtol(argv[2], &invalChar2, 10)) >= INT_MAX))
	{
		fprintf(stderr, "Overflow. Both arguments (%s and %s) must be positive integers less than %d\n", argv[1], argv[2], INT_MAX);
		exit(1);
	}

	//check that arguments are valid integers > 0 also checks underflow error and that there are no characters in the arguments
	if(!(arg1 >= 0 && arg2 >= 0) || (*invalChar1 || *invalChar2))
	{
		fprintf(stderr, "Both arguments must be valid positive integers greater than or equal to 0. The arguments: %s, and %s, do not meet these requirements.\n", argv[1], argv[2]);
		exit(1);
	}

	//set connections
	conn = (int)arg1;
	conn2 = (int)arg2;

	char msg[MSG_LENGTH];
	memset(msg, 0, MSG_LENGTH);

	msg[0] = 'm';
	strcat(msg, "99,1357");//initial board setting
	strcat(msg, "-");

	writeData(conn, msg);

	//get reply from first, ask second for move, get reply ask first for move
	while(stillPlaying)
	{
		if(readData(conn, msg) == 0)
		{
			stillPlaying = 0;

			//tell other play that opponent quit
			msg[0] = 'q';
			strcat(msg, "-");
			writeData(conn2, msg);
		}
		else
		{
			strcat(msg, "-");
			if(writeData(conn2, msg) != 1)
			{
				stillPlaying = 0;

				//tell other play that opponent quit
				msg[0] = 'q';
				strcat(msg, "-");
				writeData(conn, msg);
			}
			else
			{
				if(readData(conn2, msg) == 0)
				{
					stillPlaying = 0;

					//tell other play that opponent quit
					msg[0] = 'q';
					strcat(msg, "-");
					writeData(conn, msg);
				}
				else
				{
					strcat(msg, "-");
					if(writeData(conn, msg) != 1)
					{
						stillPlaying = 0;

						//tell other play that opponent quit
						msg[0] = 'q';
						strcat(msg, "-");
						writeData(conn2, msg);
					}
				}
			}
		}
	}

	exit(0);
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
			perror("nim_match_server:write");
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
