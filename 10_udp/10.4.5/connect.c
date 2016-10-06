#include <sys/types.h>
#include <sys/socket.h>	/*socket()/bind()*/
#include <netinet/in.h>	/*struct sockaddr_in*/
#include <string.h>		/*memset()*/
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PORT_SERV 8888
int main(int argc, char*argv[])
{
	int s;												/*套接字文件描述符*/
	struct sockaddr_in addr_serv;						/*服务器地址*/
	struct sockaddr_in local;							/*本地地址*/
	socklen_t len = sizeof(local);							/*地址长度*/
		
	s = socket(AF_INET, SOCK_DGRAM, 0);				/*生成数据报套接字*/
	
	/*填充服务器地址*/
	memset(&addr_serv, 0, sizeof(addr_serv));			/*清零*/
	addr_serv.sin_family = AF_INET;					/*AF_INET协议族*/
	addr_serv.sin_addr.s_addr =inet_addr("127.0.0.1"); /*地址为127.0.0.1*/
	addr_serv.sin_port = htons(PORT_SERV);				/*服务器端口*/
		
	connect(s, (struct sockaddr*)&addr_serv, sizeof(addr_serv));
														/*连接服务器*/	
	getsockname(s, (struct sockaddr*)&local, &len);			/*获得套接字文件描述符的地址*/	
	printf("UDP local addr:%s\n",inet_ntoa(local.sin_addr));
														/*打印获得的地址*/
	
	close(s);
	return 0;	
}