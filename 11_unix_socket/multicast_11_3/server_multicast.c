/*
*broadcast_server.c - 多播服务程序
*/

#include <unistd.h>
#include <stdio.h>
#include <signal.h>			// signal
#include <string.h>			// strlen strstr memset memcpy
#include <errno.h>			// errno
#include <fcntl.h>			// fcntl F_SETFL O_NONBLOCK

#include <netinet/in.h>		// 	socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <net/if.h>			//  IFNAMSIZ   struct ifreq
#include <linux/sockios.h>	//  SIOCGIFADDR 网卡的IP地址  SIOCGIFBRDADDR网卡的广播地址
#include <arpa/inet.h>		// 	inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
							//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
							//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现


#define MCAST_PORT 8888						// 组播端口??
#define MCAST_ADDR "224.0.0.88" 			// 组播地址 一个局部连接多播地址，路由器不进行转发
#define MCAST_DATA "BROADCAST TEST DATA"
#define MCAST_INTERVAL 5
int main(int argc, char*argv[])
{
	int s;
	struct sockaddr_in mcast_addr;		
	s = socket(AF_INET, SOCK_DGRAM, 0);			/*建立套接字*/
	if (s == -1)
	{
		perror("socket()");
		return -1;
	}
	
	memset(&mcast_addr, 0, sizeof(mcast_addr));
	mcast_addr.sin_family = AF_INET;
	mcast_addr.sin_addr.s_addr = inet_addr(MCAST_ADDR);
	mcast_addr.sin_port = htons(MCAST_PORT);

	while(1) {
		printf("sendto\n");
		int n = sendto(s, MCAST_DATA, sizeof(MCAST_DATA),
							0, (struct sockaddr*)&mcast_addr,  sizeof(mcast_addr));
		if( n < 0){
			perror("sendto error");
			return -2;
		}		
		sleep(MCAST_INTERVAL);
	}
	
	return 0;
}
