#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write
#include<poll.h>
#include <sys/time.h>

#define CARNUM 4
#define MAX 100

typedef struct path_info{

	int direction;
	char path_name[20];
	int distance;
}path_info;

typedef struct msg{

	int IDs[CARNUM];
	int sequences[CARNUM];
}msg;

typedef struct packet{
	int type; //0:path request(from laeader) 1:path reply, 2:notification 3:ACK
	//from, to or ttl are discarded. because of not our section.
	path_info p_Info[3];
	msg message;
	int from;
}packet;

void pathMapping(path_info info, char* t);
void order_algorithm(); //차 정렬 알고리즘
void printPacket(packet);
void openServer();
void openClient();
void* server_thread(void *arg);
void* client_thread(void *arg);
void init(char* IP, char* ID, char* port); //initiate IP(or isLeader) and ID
void* timer();
packet make_ACK_packet(packet p);

packet server_recv_packet;
packet server_send_packet;
packet client_recv_packet;
packet client_send_packet;
packet myPacket;
pthread_t tid[3]; //0:cleint(앞차랑 연결), 1:server(뒷차랑 연결), 2:timer

char myIP[16];
char frontIP[16]="20.20.1.130"; //앞차의 IP 
char rearIP[16]; //뒷차의 IP즘
path_info myPath[MAX]; //나의 path

//timer
struct timeval prev, now;
double prev_mill, now_mill;

int isSCH;

packet packetQueue[CARNUM];
//path_info Store_input[10]; //프로그램 시작 시 나의 path를 파일로 부터 입

int recvNum = 1;
int numOfCar = CARNUM;
int carIDs[CARNUM];
int pathRecvCheck; //numOfCar와 같아지면 pathRecv다 온거다.
int current_path_location;
int count = 0;

int myCarId; //차량 이름 (혹은 번호) (고유해야함)
int serverSock;
int clientSock;
int isLeader = 0;
int isBackAvail = 0;
int isFrontAvail = 0;
int isMyPacket = 0;
int isBackChange = 0;
int type0_flag = 0;
int isClient_flag = 0;
int result[CARNUM] = {0,};
int PORT;

int IDs[100]; //리더로 부터 오는 정보를 임시 저장하기 위해

int main(int argc, char* argv[])
{
	int choice;
	FILE* f;
	int err_timer;

	if(argc != 4)
	{
		puts("param1 should be forntIP, param2 should be myCarId, param3 is PortNum");
		return;
	}	
	init(argv[1], argv[2], argv[3]);

	if( (f = fopen("input.txt","r")) == 0 )
	{
		printf("파일이 없습니다.\n");
		exit(1);
	}

	while(!feof(f))
	{
		fscanf(f,"%d %s %d", &myPath[++count].direction, myPath[count].path_name, &myPath[count].distance);		
	}

	err_timer = pthread_create(&(tid[2]), NULL, &timer, NULL);

	if (err_timer != 0)
		printf("\ncan't create thread :[%s]", strerror(err_timer));
	else
		printf("\n Timer Thread created successfully\n");



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
				pathRecvCheck=1;
			}
			else if(choice == 2) {
				type0_flag = 1;
				recvNum = 1;
				server_send_packet.type = 2;
			}
		}
	}
}


void* timer() {	//mj
	gettimeofday(&prev, NULL);
	prev_mill = (prev.tv_sec) * 1000 + (prev.tv_usec) / 1000;

	while (1) {
		gettimeofday(&now, NULL);
		now_mill = (now.tv_sec) * 1000 + (now.tv_usec) / 1000;

		if (now_mill - prev_mill >= 50) {
			//printf("%.0lf %d\n",prev_mill, isSCH);
			prev_mill = now_mill;
			prev = now;
			if (isSCH == 0)
				isSCH = 1;
			else
				isSCH = 0;
		}
	}

}

packet make_ACK_packet(packet p) {
	p.type = 3;
	p.from = myCarId;
	return p;
}


void init(char* IP, char* ID, char* port)
{
	if(strcmp("L", IP)==0)
	{
		isLeader = 1;
		strcpy(frontIP, "Leader");

	}
	else
		strcpy(frontIP, IP);
	
	myCarId = atoi(ID);

	if(isLeader)
			carIDs[0] = myCarId;

	PORT = atoi(port);

	printf("car initiate!! frontIP:%s, myCarID:%d, port:%d\n", frontIP, myCarId, PORT);
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
	fd_set readfds;
	struct pollfd fd;
	int ret;
	int k;

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

	//accept connection from an incoming client
	client_sock = accept(serverSock, (struct sockaddr *)&client, (socklen_t*)&c);
	if (client_sock < 0)
	{
		perror("accept failed");
		return; //안된다고 죽나...??
	}
	else
		puts("서버에 클라이언트(후방차량)이 접속했습니다.");

	while(1)
	{
		if (!isSCH) continue;
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
			}
			else {
				printPacket(server_recv_packet);
				if(isLeader) //리더 앞에 그룹원이 없으니까.
				{			
					if(server_recv_packet.type == 1)
					{
						//puts("testsetestsetse");
						//printPacket(server_recv_packet);
						memcpy(&packetQueue[pathRecvCheck], &server_recv_packet, sizeof(packet));
						pathRecvCheck++;
						/*
						pathQueue[pathRecvCheck][0] = server_recv_packet.p_Info[0];
						pathQueue[pathRecvCheck][1] = server_recv_packet.p_Info[1];
						pathQueue[pathRecvCheck++][2] = server_recv_packet.p_Info[2];	
						*/
						if(pathRecvCheck == CARNUM)
						{
							puts("path 모두 도착!");
							order_algorithm();
						}					
					} 
				}else{ //리더가 아니면 앞으로 넘겨준다.
					printf("temptemp: %d",temptemp);
					switch(server_recv_packet.type){
						case 1 :
							client_send_packet.type = server_recv_packet.type;
							for (k = 0; k < 3; k++)
							{
								if(server_recv_packet.p_Info[k].direction == -1) break;
								strcpy(client_send_packet.p_Info[k].path_name, server_recv_packet.p_Info[k].path_name);
								client_send_packet.p_Info[k].direction = server_recv_packet.p_Info[k].direction;
								client_send_packet.p_Info[k].distance = server_recv_packet.p_Info[k].distance;
							}
							client_send_packet.from = server_recv_packet.from;
							break;
						case 3 : 
							client_send_packet.type = server_recv_packet.type;
							client_send_packet.from = server_recv_packet.from;
							break;
					}
					isFrontAvail = 1;
					
				}
				//if(server_recv_packet.type!=0)
				//	printPacket(server_recv_packet);
			}
			
		}

		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(isBackAvail) //isBackAvail이 1이라는 말은 클라이언트가 받았다라고 봐도 된다.
		{
			
			if( send(client_sock , &server_send_packet , sizeof(server_send_packet) , 0) < 0)			
				puts("Send failed");
			
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

		//isClient_flag = 1;

		if(type0_flag) //path requset!. this if statement only be excuted if this car is the Leader
		{
			if( send(client_sock , &server_send_packet, sizeof(server_send_packet) , 0) < 0)
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

void order_algorithm()
{
	int i, j, k;
	
	char diffs[50][50]={0,}; //리더와 다른 step을 저장.
	int num[50]={0,}; //counting sort 그룹당 원소 개수 
	int location[50]={0,}; //counting sort 위치
	char leader_step[100]={0,}; //리더의 스텝 temp

	char transfered_path[50][100]; //step값을 direction + path_name + distance 한 문자열의 모임(비리더들의 step을 모아서 보관)
	int IDs[50];
	int cnt=1; //리더와 step이 다른 개수 (단 중복 안됨) (예를 들어 A(리더) A B B C 이면 cnt는 2 ) 즉, cnt가 1이상이면 그게 곧 서브 그룹의 개수

	IDs[0] = myCarId;
	for(i=1; i<CARNUM; ++i)	
		IDs[i] = packetQueue[i].from;

	for(i=0; i<1 && cnt==1; ++i)
	{
		int u;

		for(u=1; u<CARNUM; ++u)		
			sprintf(transfered_path[u],"%d%s%d"
						,packetQueue[u].p_Info[i].direction
						,packetQueue[u].p_Info[i].path_name
						,packetQueue[u].p_Info[i].distance);
		
		sprintf(leader_step,"%d%s%d",packetQueue[0].p_Info[i].direction, packetQueue[0].p_Info[i].path_name, packetQueue[0].p_Info[i].distance);		
		
		num[0] = 1;
		strcpy(diffs[0], leader_step);
		for(j=1; j<CARNUM; ++j)
		{
			//printf("%s와 %s 비교\n", leader_step, transfered_path[j]);			
			if(strcmp(leader_step, transfered_path[j])) //리더랑 다름
			{
				int already_in = 0;
				for(k=1; k<cnt && already_in==0; k++)
				{				
					if(strcmp(diffs[k],transfered_path[j])==0)					
					{
						already_in = 1;		
						num[k]++;
						continue;
					}
				}
				
				if(!already_in)
				{				
					num[cnt] = 1;	
					strcpy(diffs[cnt++], transfered_path[j]);									
				}
			}		
			else //리더랑 같은 애는 개수만 쌓임
				num[0]++;		
		}
	}
	
	if(cnt==1)
	{
		puts("서브그룹 감지 없음.");
		return;	
	}

	for(i=1; i<cnt; ++i)
		location[i] = location[i-1] + num[i-1];
	location[0] = 1;

	for(i=1; i<CARNUM; ++i) //리더는 어짜피 맨앞이니까.
	{
		for(j=0; j<cnt; ++j)
		{
			if(!strcmp(diffs[j], transfered_path[i])) //같음!
			{
				result[ location[j]++ ] = IDs[i];
				break;
			}
		}
	}

	puts("서브그룹 감지!!");
	result[0] = myCarId;
	for(i=0; i<CARNUM; ++i) //리더는 어차피 맨앞이니까.	
		printf("%d ",result[i]);
	puts("");	
}

void pathMapping(path_info info, char* t)
{
	char temp1[20]={0,};
	char temp2[20]={0,};
	int i;

	for(i=0; i<100; ++i)
		t[i] = 0;
	sprintf(t, "%d%s%d", info.direction, info.path_name, info.direction);
	/*
	sprintf(temp1, "%d", info.direction);
	sprintf(temp2, "%d", info.distance);
	//itoa(info.direction, temp1, 10);
	strcat(t, temp1);
	strcat(t, info.path_name);
	//itoa(info.distance, temp2, 10);
	strcat(t, temp2);
	*/
}

void* client_thread(void *arg)
{	
	int state;
	int myPath_current=1;
	struct sockaddr_in server;
	fd_set readfds;
	struct pollfd fd;
	int ret;
	int k;

	clientSock = socket(AF_INET , SOCK_STREAM , 0);

	if (clientSock == -1)	
		printf("Could not create socket");
	
	puts("Socket created");

	server.sin_addr.s_addr = inet_addr(frontIP);
	server.sin_family = AF_INET;
	server.sin_port = htons( PORT );

	//Connect to remote server
	if (connect(clientSock , (struct sockaddr *)&server , sizeof(server)) < 0)	
		perror("connect failed. Error");	
	else
		puts("clientSock: Connected\n");

	//keep communicating with server
	while(!isLeader)
	{
		if(!isSCH) continue;

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
				printf("switch before\n");
				switch(client_recv_packet.type){
					case 0:
						printf("switch after\n");
						server_send_packet.type = client_recv_packet.type;

						client_send_packet.type = 1;	
						for (k = 0; k < 3; k++)
						{
							if (myPath_current + k > count) break;
							strcpy(client_send_packet.p_Info[k].path_name, myPath[myPath_current + k].path_name);
							client_send_packet.p_Info[k].direction = myPath[myPath_current + k].direction;
							client_send_packet.p_Info[k].distance = myPath[myPath_current + k].distance;
						}
						myPath_current++;
						client_send_packet.from = myCarId;

						isClient_flag = 1;
						break;

					case 2 :
						server_send_packet.type = client_recv_packet.type;
						for(k = 0;k < 10;k++)
							server_send_packet.message.IDs[k] = client_recv_packet.message.IDs[k];
						for(k = 0;k < 10;k++)
							server_send_packet.message.sequences[k] = client_recv_packet.message.sequences[k];
						
						client_send_packet = make_ACK_packet(client_send_packet);
						isFrontAvail = 1;
						break;
				} //end of switch
				puts("recv something");
				//puts(recv_packet.path[0]);
				isBackAvail = 1;
			}
		}

		//입력이 들어오든 안들어오든 뒷차로 쏘아줄 정보가 있는지
		if(isClient_flag)
		{
		
			if( send(clientSock , &client_send_packet , sizeof(client_send_packet) , 0) < 0)			
				puts("Send failed");
			else	
				puts("client_send_packet success_flag");
			isClient_flag = 0;
		}
		
		if( isFrontAvail )
		{
			if( send(clientSock, &client_send_packet, sizeof(client_send_packet), 0) < 0)			
				puts("Send failed");
			else
				puts("client_send_packet success_FrontAvail");
			
			isFrontAvail = 0;
		}
	}
	close(clientSock);
}

void printPacket(packet p)
{
	int i;
	puts("======================PACKET======================");
	printf("type:%d from:%d\n",p.type, p.from);
	if(p.type == 1)	
	{
		puts("--------------path--------------");
		for(i=0; i<3; ++i)
			printf("direction:%d, where: %s distance:%d\n", p.p_Info[i].direction, p.p_Info[i].path_name, p.p_Info[i].distance);
	}

	else if(p.type == 2)
	{
		puts("--------------notification--------------");
		for(i=0; i<CARNUM; ++i)
			printf("%d	", p.message.IDs[i]);
		puts("");
	}
	else if (p.type == 3)
		puts("received type3");
	puts("=======================END=======================");
}
