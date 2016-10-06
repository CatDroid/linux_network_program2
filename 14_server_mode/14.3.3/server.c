#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#define BUFFLEN 1024
#define SERVER_PORT 8888
#define BACKLOG 5
#define CLIENTNUM 2
/*互斥量*/
pthread_mutex_t ALOCK = PTHREAD_MUTEX_INITIALIZER;
static void *handle_request(void *argv)
{	
	int s_s = *((int*)argv);
	int s_c;								/*客户端套接字文件描述符*/
	struct sockaddr_in from;				/*客户端地址*/
	socklen_t len = sizeof(from);
	for(;;)
	{
		time_t now;							/*时间*/
		char buff[BUFFLEN];					/*收发数据缓冲区*/
		int n = 0;
		
		pthread_mutex_lock(&ALOCK);			/*进入互斥区*/
		s_c = accept(s_s, (struct sockaddr*)&from, &len);
		/*接收客户端的请求*/
		pthread_mutex_unlock(&ALOCK);		/*离开互斥区*/
		
		memset(buff, 0, BUFFLEN);			/*清零*/
		n = recv(s_c, buff, BUFFLEN,0);	/*接收发送方数据*/
		if(n > 0 && !strncmp(buff, "TIME", 4))	/*判断是否合法接收数据*/
		{
			memset(buff, 0, BUFFLEN);		/*清零*/
			now = time(NULL);				/*当前时间*/
			sprintf(buff, "%24s\r\n",ctime(&now));	/*将时间复制入缓冲区*/
			send(s_c, buff, strlen(buff),0);		/*发送数据*/
		}		
		/*关闭客户端*/
		close(s_c);	
	}
	
	return NULL;
}
static void handle_connect(int s)
{	
	int s_s = s;
	pthread_t  thread_do[CLIENTNUM];		/*线程ID*/
	int i = 0;
	for(i=0;i<CLIENTNUM;i++)				/*建立线程池*/
	{
		/*创建线程*/
		pthread_create(&thread_do[i],		/*线程ID*/
					NULL,					/*属性*/
					handle_request,			/*线程回调函数*/
					(void*)&s_s);			/*线程参数*/
	}
	/*等待线程结束*/
	for(i=0;i<CLIENTNUM;i++)
		pthread_join(thread_do[i], NULL);
}
int main(int argc, char *argv[])
{
	int s_s;								/*服务器套接字文件描述符*/
	struct sockaddr_in local;				/*本地地址*/	
	
	/*建立TCP套接字*/
	s_s = socket(AF_INET, SOCK_STREAM, 0);
	
	/*初始化地址和端口*/
	memset(&local, 0, sizeof(local));		/*清零*/
	local.sin_family = AF_INET;				/*AF_INET协议族*/
	local.sin_addr.s_addr = htonl(INADDR_ANY);	/*任意本地地址*/
	local.sin_port = htons(SERVER_PORT);		/*服务器端口*/
	
	/*将套接字文件描述符绑定到本地地址和端口*/
	bind(s_s, (struct sockaddr*)&local, sizeof(local));
	listen(s_s, BACKLOG);					/*侦听*/
	
	/*处理客户端连接*/
	handle_connect(s_s);
	
	close(s_s);									/*关闭套接字*/
	
	return 0;
}