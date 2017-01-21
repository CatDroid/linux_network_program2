
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

#define IP_FOUND "IP_FOUND"        			/*IP发现命令*/
#define IP_FOUND_ACK "IP_FOUND_ACK"			/*IP发现应答命令*/
#define MCAST_PORT 10010

unsigned char exitflag = 0 ;

static void sigint_handler(int signo){
	exitflag = 1 ;
	return;
}

//#define BIND_DEVICE
int main(int argc, char *argv[]){

	int ret = -1;
	int sock = -1;

	sock = socket(AF_INET, SOCK_DGRAM, 0);	/*建立数据报套接字*/
	if( sock < 0 ){
		perror("create socket error");
		return -1 ;
	}

	int rcv_size = 4096;
	int optlen = sizeof(rcv_size);
	ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcv_size, optlen);
	if(ret < 0){
		printf("setsockopt SO_RCVBUF %d %s\n",errno, strerror(errno));
		return -1 ;
	}
	rcv_size = 0 ;
	ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen);
	if(ret < 0){
		printf("setsockopt SO_RCVBUF %d %s\n",errno, strerror(errno));
		return -1 ;
	}
	printf("receive buffer = %d \n" , rcv_size);

	exitflag = 0 ;

	/*服务端 绑定端口  */
#ifdef BIND_DEVICE
	#define IFNAME "usb0"
	struct ifreq ifr;
	strncpy(ifr.ifr_name,IFNAME,strlen(IFNAME));
	if(ioctl(sock,SIOCGIFADDR,&ifr) == -1){
		perror("ioctl error"); // ioctl error: No such device 如果没有对应网络接口
		return -1 ;
	}
#endif


	struct sockaddr_in local_addr;
	memset((void*)&local_addr, 0, sizeof(struct sockaddr_in));
#ifdef BIND_DEVICE
	memcpy(&local_addr, &ifr.ifr_broadaddr, sizeof(struct sockaddr_in ));
#else
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// 对本地任何一个网卡
#endif
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(MCAST_PORT);

	ret = bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));
	if(ret != 0){
		perror("bind error");
		return -1 ;
	}
	printf("receiver bind to %s %d\n", inet_ntoa(local_addr.sin_addr), MCAST_PORT);


	signal(SIGINT, sigint_handler);
	fcntl(sock,F_SETFL, O_NONBLOCK);

	struct sockaddr_in from_addr;
	memset(&from_addr, 0 , sizeof(struct sockaddr_in) );


#define BUFFER_LEN 200
	char buff[BUFFER_LEN];

	while(!exitflag)
	{
		memset(buff , 0 , BUFFER_LEN);
		int from_len = sizeof(struct sockaddr_in);
		int count = recvfrom( sock, buff, BUFFER_LEN, MSG_TRUNC,
							(struct sockaddr*) &from_addr, &from_len );
		if( count > BUFFER_LEN){
			printf("消息过大 已被截断 count = %d \n", count);
			//buff[BUFFER_LEN - 1 ] = '\0';
		}else if(count == BUFFER_LEN) {
			//buff[BUFFER_LEN - 1 ] = '\0';
		}else if( count > 0 ){
			//buff[count] = '\0' ;
		}else {
			if(errno == EAGAIN){
				/*
				 *  会覆盖覆盖原来接受的
				 * */
				printf("try again in 38 sec\n");
				sleep(5);
				continue ;
			}else{
				printf("recvfrom error %d %s \n", errno ,strerror(errno));
				break;
			}
		}
		printf("%d %c %c %c \n" , *(int*)buff ,
									buff[BUFFER_LEN-3] ,
									buff[BUFFER_LEN-2] ,
									buff[BUFFER_LEN-1] );
		sleep(5);

	}

	close(sock);
	return 0 ;
}
