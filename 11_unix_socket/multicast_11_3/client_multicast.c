/*
 * client_multicast.c
 *
 *  Created on: 2016年10月22日
 *      Author: hanlon
 */


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
#include <netinet/in.h>		// 	socket  AF_INET SOCK_RAW  IPPROTO_ICMP

#define MCAST_PORT 8888
#define MCAST_ADDR "224.0.0.88" 			// 组播地址 一个局部连接多播地址，路由器不进行转发
//#define LOCAL_PORT 9996


int main(int argc, char *argv[]){

	int ret = -1;
	int sock = -1;

	// 建立数据报套接字 只有UDP可以发送广播
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock < 0 ){
		perror("create socket error");
		return -1 ;
	}


	struct sockaddr_in local_addr;

	local_addr.sin_port = htons(MCAST_PORT);
	local_addr.sin_family = AF_INET ;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 如果发送组播的话 从默认网卡发送出去

	ret = bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));
	if(ret != 0){
		perror("bind error");
		return -1 ;
	}// 不能不绑定端口

	printf("client bind to %s %d\n", inet_ntoa(local_addr.sin_addr), MCAST_PORT);

	int so_loop = 1;
	ret = setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,&so_loop,sizeof so_loop);
	if(ret != 0){
		perror("setsockopt IP_MULTICAST_LOOP error");
		return -1 ;
	}

	struct ip_mreq mreq;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY) ;
	mreq.imr_multiaddr.s_addr = inet_addr(MCAST_ADDR);
	ret = setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof mreq);

	int count = -1;
	int times = 10;
	int i = 0;
#define BUFFER_LEN 32
	char buff[BUFFER_LEN];

	for( i = 0 ; i < times ; i++ )
	{
		struct sockaddr_in from_addr;
		int from_len = sizeof(struct sockaddr_in);
		count = recvfrom( sock, buff, BUFFER_LEN, MSG_TRUNC,
					( struct sockaddr*) &from_addr, &from_len );

		if( count > BUFFER_LEN){
			printf("消息过大 已被截断\n");
			buff[BUFFER_LEN - 1 ] = '\0';
		}else if(count == BUFFER_LEN) {
			buff[BUFFER_LEN - 1 ] = '\0';
		}else{
			buff[count] = '\0' ;
		}
		// 这里from_addr 不是组播的地址 而是发送者的地址 和 端口 信息
		printf( "Recv msg is %s from %s %d \n", buff , inet_ntoa(from_addr.sin_addr) , ntohs(from_addr.sin_port) );
	}

	ret = setsockopt(sock,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof mreq);

	close(sock);
	return 0 ;
}
