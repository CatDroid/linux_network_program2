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
#include <sys/socket.h>		// socket


#define MCAST_PORT 10010
//#define IFNAME "eth0"
//#define IFNAME "usb0"
//#define IFNAME "p2p0"
#define IFNAME "wlan0"

int exit_flag = 0 ;

static void sigint_handler(int signo){
	exit_flag = 1 ;
	return;
}


int main(int argc, char *argv[]){

	int ret = -1;
	int sock = -1;
	exit_flag = 0 ;

	// 建立数据报套接字 只有UDP可以发送广播
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock < 0 ){
		perror("create socket error");
		return -1 ;
	}

	int snd_size = 4096;
	int optlen = sizeof(snd_size);
	ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(ret < 0){
		printf("setsockopt SO_SNDBUF %d %s\n",errno, strerror(errno));
		return -1 ;
	}


	// 获得指定网卡的广播地址
	// SIOCGIFADDR		获得某个网卡的IP地址	返回ifr.ifr_addr			类型是 struct sockaddr
	// SIOCGIFBRDADDR	获得某个网卡的广播地址 	返回ifr.ifr_broadaddr	类型是 struct sockaddr
	struct ifreq ifr;
	//memset(&ifr , 0 , sizeof(struct ifreq)); // 没有这个可能错误,或者下面用sizeof包含\0
	strncpy(ifr.ifr_name,IFNAME,sizeof(IFNAME)); // 不能用strlen 要包含\0
	if(ioctl(sock,SIOCGIFADDR,&ifr) == -1){
		printf("ioctl error %d %s %s\n",errno, strerror(errno), ifr.ifr_name); // ioctl error: No such device 如果没有对应网络接口
		return -1 ;
	}


	/**
	 *	如果目的地是本地网卡IP
	 *	那么wireshark中是捕捉不了数据的 可能就根本不会从网卡出去
	 *	而且会收到ICMP
	 *
	 *	但是如果目的是其他主机的网卡 就收不到ICMP(???要看路由器是否回复??)
	 */
	struct sockaddr_in target_addr;
	memcpy(&target_addr, &ifr.ifr_addr , sizeof(struct sockaddr_in )); // 目的地是本地网卡
	//target_addr.sin_addr.s_addr = inet_addr("192.168.43.122");
	target_addr.sin_port = htons(MCAST_PORT);
	target_addr.sin_family = AF_INET ;
	printf("sennd to %s %d\n", inet_ntoa(target_addr.sin_addr), MCAST_PORT );


	signal(SIGINT, sigint_handler);

	/*主处理过程*/
	fcntl(sock,F_SETFL, O_NONBLOCK);


#define BUFFER_LEN 256
	char buff[BUFFER_LEN];
	int i = 0;
	for( ; i < BUFFER_LEN ; i++ ){
		buff[i] = 'a' +  i % 26 ;
	}
	buff[200 - 3] = 'e' ;
	buff[200 - 2] = 'n' ;
	buff[200 - 1] = 'd' ;

	struct sockaddr_in from_addr;		/*服务器端地址*/

	fd_set readfd;
	int count = -1;
	int packet_num = 0 ;
	while(!exit_flag)
	{

		*(int*)buff = packet_num++ ;
		printf("send %d to %s %d %d\n" ,*(int*)buff ,
													inet_ntoa( target_addr.sin_addr ),
										 	 	 	 ntohs( target_addr.sin_port) ,
										 	 	 	target_addr.sin_family );

		ret = sendto(sock, buff, BUFFER_LEN , MSG_DONTROUTE ,
							(struct sockaddr*)&target_addr,
							sizeof(target_addr)); // 往IP广播地址+UDP端口MCAST_PORT,发送广播
		if(ret < 0 ){
			printf("sendto error %d %s \n" ,errno ,strerror(errno));
			break;
		}else{
			sleep(1);
		}
		if(packet_num == 90){
			break;
		}
	}	
	close(sock);
	return 0 ;
}
