#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>
 
#define BACKLOG 2						// 侦听队列长度 

void process_conn_server(int s)
{
	ssize_t size = 0;
	char buffer[1024];
	for(;;){									
		size = read(s, buffer, 1024);	
		if(size == 0){	
			printf("close client %d\n" , s);
			close(s);
			return;	
		}
		sprintf(buffer, "%zd bytes altogether\n", size); // 通知对方收取了多少字节数据
		write(s, buffer, strlen(buffer)+1);
	}
}

void sig_chld(int signo)
{
	pid_t pid;
	int stat;
	pid=wait(&stat);
	printf("child %d exit \n", pid);
	return;
}
	
int main(int argc, char *argv[])
{
	int ss,sc;		 
	struct sockaddr_in server_addr;	 
	struct sockaddr_in client_addr;	 
	int err;							 
	pid_t pid;							 

	if(argc != 2){
		printf("usage: ./%s port\n" , argv[0] );
		return -1;
	}
 
	ss = socket(AF_INET, SOCK_STREAM, 0);
	if(ss < 0){						 
		printf("socket error\n");
		return -1;	
	}
	
 
	bzero(&server_addr, sizeof(server_addr));			 
	server_addr.sin_family = AF_INET;					 
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	 
	server_addr.sin_port = htons( atoi(argv[1]) );				 
	
	/*绑定地址结构到套接字描述符*/
	err = bind(ss, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(err < 0){/*出错*/
		printf("bind error\n");
		return -1;	
	}
	
	/*设置侦听*/
	err = listen(ss, BACKLOG);
	if(err < 0){										/*出错*/
		printf("listen error\n");
		return -1;	
	}
	
	signal(SIGCHLD, sig_chld);
	
	//sleep(100); 
	/*
		测试 全连接队列   已完成队列的大小 = min ( backlog , /proc/sys/net/core/somaxconn = 128  )
		net.ipv4.tcp_abort_on_overflow
		tcp_abort_on_overflow = 0，忽视ACK包，server过一段时间再次发送syn+ack给client 
									,也就是重新走握手的第二步 ，如果client超时等待比较短，就很容易异常了
		tcp_abort_on_overflow = 1, 直接回RST包,结束连接.
		*/
		
	/*主循环过程*/
	for(;;)	{
		socklen_t addrlen = sizeof(struct sockaddr);
		
		sc = accept(ss, (struct sockaddr*)&client_addr, &addrlen); 
		if(sc < 0){	
			printf("accpet error %d %s\n",errno, strerror(errno));
			continue;					 
		}	
		printf("accpet one client \n");
		
 
		pid = fork();	// 分叉进程					 
		if( pid == 0 ){						 
			process_conn_server(sc);		 
			close(ss);
			return 0;
		}else{
			printf("fork process %d to handle %s:%d\n" , pid , inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port) );
			close(sc);						 
		}
	}
}



