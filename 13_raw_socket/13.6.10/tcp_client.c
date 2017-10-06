#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>


#include <sys/socket.h>
#include <arpa/inet.h>
 

void process_conn_client(int s)
{
	printf("connected to server\n");
	ssize_t size = 0;
	char buffer[1024];							 
	
	for(;;){ 
		size = read(STDIN_FILENO, buffer, 1024);
		if(size > 0){
			if(!strncmp(buffer, "exit" ,strlen("exit"))){
				printf("client exit\n");
				return ;
			}
			write(s, buffer, size);		 
			size = read(s, buffer, 1024);
			write(STDOUT_FILENO, buffer, size);		 
		}
	}	
}
 

int main(int argc, char *argv[])
{
	int s;										 
	struct sockaddr_in server_addr;			 
	
	if(argc != 3){
		printf("usage: ./%s server_ip server_port\n" , argv[0] );
		return -1;
	}
	
	s = socket(AF_INET, SOCK_STREAM, 0); 	 
	if(s < 0){						 
		printf("socket error\n");
		return -1;
	}
	
 
	bzero(&server_addr, sizeof(server_addr));	 
	server_addr.sin_family = AF_INET;					  
	server_addr.sin_port = htons( atoi(argv[2]) );		 
	
	/*将用户输入的字符串类型的IP地址转为整型*/
	inet_pton(AF_INET, argv[1], &server_addr.sin_addr);	
	/*连接服务器*/
	int ret = connect(s, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
	if(ret < 0 ) {
		printf("connect error %d %s\n" , errno, strerror(errno) );
		close(s);
		return -1;
	}
	process_conn_client(s);						/*客户端处理过程*/
	close(s);									/*关闭连接*/
	return 0;
}

