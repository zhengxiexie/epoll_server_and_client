#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>

#define MAXLINE 4096
int sockfd;


void *my_thread1()
{
	int i;
	char sendline[4096];
	printf("send msg to server: \n");
	for(i=0;i<100000;i++)
	{
		sprintf(sendline, "141 t1\n"); //如果客户端程序在不同的主机上跑，可以修改这行打印语句
		if( send(sockfd, sendline, strlen(sendline), 0) < 0)
		{
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
			exit(0);
		}
		sleep(2);
	}
}

void *my_thread2()
{
	int j;
	char sendline[4096];
	printf("send msg to server: \n");
	for(j=0;j<100000;j++)
	{
		sprintf(sendline, "141 t2\n");
		if( send(sockfd, sendline, strlen(sendline), 0) < 0)
		{
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
			exit(0);
		}
		sleep(5);
	}
}

int main(int argc, char** argv)
{
	int n;
	struct sockaddr_in servaddr;
    int i,j;
	int ret=0;
	pthread_t id1,id2;


	if( argc != 2){
		printf("usage: ./client <ipaddress>\n");
		exit(0);
	}

	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
		exit(0);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(6666);
	if( inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
	{
		printf("inet_pton error for %s\n",argv[1]);
		exit(0);
	}

	if( connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
		exit(0);
	}

	ret = pthread_create(&id1, NULL, (void*)my_thread1, NULL);
	if (ret)
	{
		printf("Create pthread error!\n");
		return 1;
	}

	ret = pthread_create(&id2, NULL, (void*)my_thread2, NULL);
	if (ret)
	{
		printf("Create pthread error!\n");
		return 1;
	}

	pthread_join(id1, NULL);
	pthread_join(id2, NULL);

	close(sockfd);

	
	exit(0);
}
