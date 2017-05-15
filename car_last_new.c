#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<poll.h>

#define CARNUM 10
#define PORT 8889

typedef struct path{

	char path_name[50];
}path;

typedef struct msg{

	int IDs[CARNUM];
	int sequences[CARNUM];
}msg;

typedef struct packet{
	int type; //0:path request(from laeader) 1:path reply, 2:notification 3:ACK
	//from, to or ttl are discarded. because of not our section.
	char path[3][50];
	msg message;
	int from;
}packet;

void printPacket(packet);
void openServer();
void openClient();
void* server_thread(void *arg);
void* client_thread(void *arg);
void init(char* IP, char* ID); //initiate IP(or isLeader) and ID

packet server_recv_packet;
packet server_send_packet;
packet client_recv_packet;
packet client_send_packet;
packet myPacket;
pthread_t tid[2]; //0:cleint(앞차랑 연결), 1:server(뒷차랑 연결)

char myIP[16];
char frontIP[16]="20.20.1.130"; //앞차의 IP 
char rearIP[16]; //뒷차의 IP
path myPath[100]; 

char cnt = 'A';
int myCarId; //차량 이름 (혹은 번호) (고유해야함)
int serverSock;
int clientSock;
int isLeader;
int isBackAvail;
int isFrontAvail;
int isMyPacket;
int isBackChange;
int type0_flag;
int isClient_flag;

int IDs[100]; //리더로 부터 오는 정보를 임시 저장하기 위해

int main(int argc, char* argv[])
{
	int err_server, err_client;
	int choice;
	
	if(argc != 3)
	{
		puts("param1 should be forntIP, param2 should be myCarId");
		return;
	}
	
	init(argv[1], argv[2]);

	openServer();
	sleep(3); //서버들이 열려야...
	openClient();
	while(1)
	{
		if(isLeader)
		{
			puts("Leader, choose one (0:type0)");
			scanf("%d",&choice);
			if(choice == 0){
				type0_flag = 1; //flag will be 0 by any Thread.
				server_send_packet.type = 0;
			}
		}
	}
}

void init(char* IP, char* ID)
{
	if(strcmp("L", IP)==0)
	{
		isLeader = 1;
		strcpy(frontIP, "Leader");
	}
	else
		strcpy(frontIP, IP);
	myCarId = atoi(ID);
	printf("%s %d\n", frontIP, myCarId);
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
		printf("\n Client Thread created successfully\n");
}

void* server_thread(void *arg)
{
	unsigned long i = 0;

	int socket_desc , client_sock , c , read_size;
	struct sockaddr_in server , client;
	struct timeval tv;
	fd_set readfds;
	struct pollfd fd;
	int ret;

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
	server.sin_port = htons( PORT );

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

	puts("yyyyyyyyyyyyyyyyyyyyyyyyyy");
	//accept connection from an incoming client
	client_sock = accept(serverSock, (struct sockaddr *)&client, (socklen_t*)&c);
	if (client_sock < 0)
	{
		perror("accept failed");
		return;
	}
		puts("nnnnnnnnnnnnnnnnnnnnnnnnnn");
	puts("Connection accepted");

	while(1)
	{
		cnt++;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		fd.fd = client_sock; // your socket handler 
		fd.events = POLLIN;
		ret = poll(&fd, 1, 1000); // 1 second for timeout

		//입력이 들어옴
		if(ret!=0 && ret!=-1) //something come from behindCar
		{
			int temptemp;
			//Receive a reply from the server
			if( (temptemp = recv(client_sock , &server_recv_packet , sizeof(server_recv_packet) , 0)) <= 0)
			{
				puts("recv failed or client disconnect!!");				
				exit(-1);
			}else {
				printf("temptemp: %d",temptemp);
				switch(server_recv_packet.type){
					case 1 :
						client_send_packet.type = server_recv_packet.type;
						strcpy(client_send_packet.path[0],server_recv_packet.path[0]);
						strcpy(client_send_packet.path[1],server_recv_packet.path[1]);
						strcpy(client_send_packet.path[2],server_recv_packet.path[2]);
						client_send_packet.from = myCarId;
						break;
					case 3 : 
						client_send_packet.type = server_recv_packet.type;
						break;
				}
				isFrontAvail = 1;

				printPacket(server_recv_packet);
			}
			
		}

		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(isBackAvail)
		{
			
			if( send(client_sock , &server_send_packet , sizeof(server_send_packet) , 0) < 0)
			{
				puts("Send failed");
			}
			puts("11111111111111111");
			isBackAvail = 0;

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
isClient_flag = 1;
		if(type0_flag) //path requset!. this if statement only be excuted if this car is the Leader
		{
			if( send(client_sock , &myPacket , sizeof(myPacket) , 0) < 0)
			{
				puts("server: Send failed");
			}
			else
				puts("server: Send Success");
			type0_flag=0;
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
	int k;

	clientSock = socket(AF_INET , SOCK_STREAM , 0);
	if (clientSock == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	server.sin_addr.s_addr = inet_addr(frontIP);
	server.sin_family = AF_INET;
	server.sin_port = htons( PORT );

	//Connect to remote server
	if (connect(clientSock , (struct sockaddr *)&server , sizeof(server)) < 0)
	{
		perror("connect failed. Error");
	}
	else
		puts("clientSock: Connected\n");

	//keep communicating with server
	while(!isLeader)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 10000;
		//FD_ZERO(&readfds);
		//FD_SET(clientSock, &readfds);
	
		fd.fd = clientSock; // your socket handler 
		fd.events = POLLIN;
		ret = poll(&fd, 1, 1000); // 1 second for timeout
		printf("ret:%d  ",ret);
		if(ret!=0 && ret!=-1)
		{
			if( recv(clientSock , &client_recv_packet , sizeof(client_recv_packet) , 0) < 0)
			{
				puts("recv failed");				
			}
			else
			{
				switch(client_recv_packet.type){
					case 0 :
						server_send_packet.type = client_recv_packet.type;
						client_send_packet.type = 1;
						strcpy(client_send_packet.path[0],"0000");
						strcpy(client_send_packet.path[1],"1111");
						strcpy(client_send_packet.path[2],"2222");
						isClient_flag = 1;
						break;
					case 2 :
						server_send_packet.type = client_recv_packet.type;
						for(k = 0;k < 10;k++)
							server_send_packet.message.IDs[k] = client_recv_packet.message.IDs[k];
						for(k = 0;k < 10;k++)
							server_send_packet.message.sequences[k] = client_recv_packet.message.sequences[k];
						break;
				}
				puts("recv something");
				//puts(recv_packet.path[0]);
				isBackAvail = 1;
			}
		}

		puts("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(isClient_flag)
		{
			if( send(clientSock , &client_send_packet , sizeof(client_send_packet) , 0) < 0)
			{
				puts("Send failed");
			}else	
				puts("client_send_packet success");
			isClient_flag = 0;
		}
		if( isFrontAvail )
		{
			if( send(clientSock, &client_send_packet, sizeof(client_send_packet), 0) < 0)
			{
				puts("Send failed");
			}else
				puts("server_send_packet success");
			
			isFrontAvail = 0;
		}
	}
	close(clientSock);
}

void printPacket(packet p)
{
	int i;
	printf("type:%d\n",p.type);
	if(p.type == 1)	
	{
		puts("--------------path--------------");
		for(i=0; i<3; ++i)
			printf("%s\n", p.path[i]);
	}

	if(p.type == 2)
	{
		puts("--------------notification--------------");
		for(i=0; i<CARNUM; ++i)
			printf("%d	", p.message.IDs[i]);
		puts("");
	}
}
