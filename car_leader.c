#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<poll.h>

typedef struct path{

	char path_name[50];
}path;

typedef struct msg{

	int *IDs;
	int *sequences;
}msg;

typedef struct packet{

	int type;
	char path[3][50];
	int GROUP_SEQUENCE;
	msg message;
}packet;

int cnt = 'A';

void openServer();
void openClient();
void* server_thread(void *arg);
void* client_thread(void *arg);

packet recv_packet;
packet send_packet;
pthread_t tid[2]; //0:cleint(앞차랑 연결), 1:server(뒷차랑 연결)

char myIP[16];
char frontIP[16]; //앞차의 IP 
char rearIP[16]; //뒷차의 IP
path myPath[100]; 
int myCarId; //차량 이름 (혹은 번호) (고유해야함)
int serverSock;
int clientSock;
int isBackAvail;
int isFrontAvail;
int isBackChange;
int IDs[100]; //리더로 부터 오는 정보를 임시 저장하기 위해

int main(void)
{
	int err_server, err_client;

	openServer();
	sleep(3); //서버들이 열려야...
	//openClient();
	while(1)
	{}
}

void openServer()
{
	int err_server;
	err_server = pthread_create(&(tid[1]), NULL, &server_thread, NULL);	

	if (err_server != 0)
		printf("\ncan't create thread :[%s]", strerror(err_server));
	else
		printf("\n Server Thread created successfully\n");
}

void openClient()
{
	int err_client;
	err_client = pthread_create(&(tid[0]), NULL, &client_thread, NULL);	

	if (err_client != 0)
		printf("\ncan't create thread :[%s]", strerror(err_client));
	else
		printf("\n Server Thread created successfully\n");
}

void* server_thread(void *arg)
{
	unsigned long i = 0;

	int socket_desc , client_sock , c , read_size;
	struct sockaddr_in server , client;
	struct timeval tv;
	fd_set readfds;
	int state;

	//Create socket
	serverSock = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 8889 );

	//Bind
	if( bind(serverSock,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return;
	}
	puts("bind done");

	//Listen
	listen(serverSock , 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);

	//accept connection from an incoming client
	client_sock = accept(serverSock, (struct sockaddr *)&client, (socklen_t*)&c);
	if (client_sock < 0)
	{
		perror("accept failed");
		return;
	}
	puts("Connection accepted");

	while(1)
	{
		cnt++;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		FD_ZERO(&readfds);
		FD_SET(client_sock, &readfds);

		state = select(client_sock, &readfds, (fd_set *)0, (fd_set *)0, &tv);

		//입력이 들어옴
		if(state !=0 && state != -1)
		{
			//Receive a reply from the server
			if( recv(client_sock , &recv_packet , sizeof(recv_packet) , 0) < 0)
			{
				puts("recv failed");				
			}
			isFrontAvail = 1;
		}

		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(!isBackAvail)
		{
			send_packet.type = 1;
			
			strcpy(send_packet.path[0],"msg");
			send_packet.path[0][3]=cnt;
			send_packet.path[0][4]='\0';

			if( send(client_sock , &send_packet , sizeof(send_packet) , 0) < 0)
			{
				puts("Send failed");
			}
			puts("11111111111111111");
			usleep(100000);
			//isBackAvail = 1;

			if(isBackChange)
			{
				client_sock = accept(serverSock, (struct sockaddr *)&client, (socklen_t*)&c);
				if (client_sock < 0)
				{
					perror("accept failed");
					return;
				}
				puts("Connection accepted");
				isBackChange=0;
			}
		}
	}

	return NULL;
}

void* client_thread(void *arg)
{	
	int state;
	struct sockaddr_in server;
	struct timeval tv;
	fd_set readfds;
	struct pollfd fd;
	int ret;

	clientSock = socket(AF_INET , SOCK_STREAM , 0);
	if (clientSock == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	server.sin_addr.s_addr = inet_addr(frontIP);
	server.sin_family = AF_INET;
	server.sin_port = htons( 8889 );

	//Connect to remote server
	if (connect(clientSock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("connect failed. Error");
	}
	else
		puts("clientSock: Connected\n");

	//keep communicating with server
	while(1)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 10000;
		FD_ZERO(&readfds);
		FD_SET(clientSock, &readfds);
	
		fd.fd = clientSock; // your socket handler 
		fd.events = POLLIN;
		ret = poll(&fd, 1, 1000); // 1 second for timeout
		printf("ret:%d  ",ret);
		if(ret!=0 && ret!=-1)
		{
			if( recv(clientSock , &recv_packet , sizeof(recv_packet) , 0) < 0)
			{
				puts("recv failed");				
			}
			else
			{
				puts("recv something");
				puts(recv_packet.path[0]);
				isBackAvail;
			}
		}

		/*
		//state = select(clientSock, &readfds, (fd_set *)0, (fd_set *)0, &tv);
		printf("state:%d       ", state);
		//입력이 들어옴
		state=3;
		if(state !=0 && state != -1)
		{
			puts("select something");
			//Receive a reply from the server
			if( recv(clientSock , &recv_packet , sizeof(recv_packet) , 0) < 0)
			{
				puts("recv failed");				
			}
			else
			{
				puts("recv something");
				puts(recv_packet.path[0]);
			}
			isBackAvail = 1;
		}
		*/
		puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(isFrontAvail)
		{
			if( send(clientSock , &send_packet , sizeof(send_packet) , 0) < 0)
			{
				puts("Send failed");
			}	
			isFrontAvail = 0;
		}
	}
	close(clientSock);
}
