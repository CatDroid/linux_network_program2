#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
extern void sig_proccess(int signo);
extern void sig_pipe(int signo);
static int s;
void sig_proccess_client(int signo)			/*客户端信号处理回调函数*/
{
	printf("Catch a exit signal\n");			/*打印信息*/
	close(s);									/*关闭套接字*/
	exit(0);									/*退出程序*/
}
#define PORT 8888								/*侦听端口地址*/
int main(int argc, char*argv[])
{
	struct sockaddr_in server_addr;			/*服务器地址结构*/
	int err;									/*返回值*/
		
	if(argc == 1){
		printf("PLS input server addr\n");
		return 0;	
	}
	signal(SIGINT, sig_proccess);				/*挂接SIGINT信号，处理函数为												  sig_process()*/
	signal(SIGPIPE, sig_pipe);	/*挂接SIGPIPE信号，处理函数为sig_pipe()*/
	
	s = socket(AF_INET, SOCK_STREAM, 0);		/*建立一个流式套接字*/
	if(s < 0){									/*建立套接字出错*/
		printf("socket error\n");
		return -1;	
	}	
	
	/*设置服务器地址*/
	bzero(&server_addr, sizeof(server_addr));	/*将地址结构清零*/
	server_addr.sin_family = AF_INET;			/*将协议族设置为AF_INET*/
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
												/*IP地址为本地任意IP地址*/
	server_addr.sin_port = htons(PORT);		/*设置服务器端口为8888*/
	
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
								/*将用户输入的字符串类型的IP地址转为整型*/
	connect(s, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
												/*连接服务器*/
	process_conn_client(s);						/*客户端处理过程*/
	close(s);									/*关闭连接*/
}
