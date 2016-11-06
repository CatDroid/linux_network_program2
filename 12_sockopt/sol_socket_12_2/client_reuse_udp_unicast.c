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


#define IP_FOUND "IP_FOUND"       			/*IP发现命令*/
#define IP_FOUND_ACK "IP_FOUND_ACK"			/*IP发现应答命令*/
#define SERVER_REUSE_PORT 18899
#define IFNAME "usb0"

void dump_my(int my_sock)
{
	int ret = 0 ;
	struct sockaddr_in my_dynamic_addr ;
	memset(&my_dynamic_addr , 0 , sizeof(my_dynamic_addr));
	int my_dynamic_addr_len = sizeof(my_dynamic_addr);
	ret = getsockname(my_sock,(struct sockaddr *)&my_dynamic_addr,&my_dynamic_addr_len);//得到本地的IP地址和端口号
	if(ret < 0 ){
		printf("getsockname my_dynamic_addr error %d %s \n", errno,strerror(errno));
	}else{
		printf("my_dynamic_addr = %s %d\n", inet_ntoa(my_dynamic_addr.sin_addr) ,
										ntohs(my_dynamic_addr.sin_port));
	}
	return ;
}

int main(int argc, char *argv[]){

	int ret = -1;
	int sock = -1;


	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock < 0 ){
		perror("create socket error");
		return -1 ;
	}

	struct ifreq ifr;
	strncpy(ifr.ifr_name,IFNAME,strlen(IFNAME));
	if(ioctl(sock,SIOCGIFADDR,&ifr) == -1){
		perror("ioctl error"); // ioctl error: No such device 如果没有对应网络接口
		return -1 ;
	}

	struct sockaddr_in unicast_addr;
	memcpy(&unicast_addr, &ifr.ifr_addr, sizeof(struct sockaddr_in ));
	unicast_addr.sin_port = htons(SERVER_REUSE_PORT);
	unicast_addr.sin_family = AF_INET ;
	printf("unicast to %s %d\n", inet_ntoa(unicast_addr.sin_addr), SERVER_REUSE_PORT );

	fcntl(sock,F_SETFL, O_NONBLOCK);

	// 创建socket后 还没有分配端口
	dump_my(sock);

	sleep(15);
	int times = 10;
	int i = 0;

#define BUFFER_LEN 32
	char buff[BUFFER_LEN];
	struct sockaddr_in from_addr;		/*服务器端地址*/

	fd_set readfd;
	int count = -1;
	for( i = 0 ; i < times ; i++ )
	{

		printf("send unicast now %s %d %d\n" ,inet_ntoa( unicast_addr.sin_addr ),
									 	 	 	 ntohs( unicast_addr.sin_port) ,
									 	 	 	unicast_addr.sin_family );
		ret = sendto(sock, IP_FOUND, strlen(IP_FOUND), 0,
							(struct sockaddr*)&unicast_addr,
							sizeof(unicast_addr)); // 往IP广播地址+UDP端口MCAST_PORT,发送广播
		// 只有在没有bind情况下 sendto的时候 才动态分配
		// 即使socket的bind的本地地址是INADDR_ANY(0.0.0.0) 但是 实际发出去的UDP报文 是有明确的源IP地址的
		dump_my(sock);
		if(ret == -1){
			perror("sendto error");
			sleep(5);
			continue;
		}
		FD_ZERO(&readfd);
		FD_SET(sock, &readfd);
		struct timeval timeout;
		timeout.tv_sec = 5;					/*超时时间2s*/
		timeout.tv_usec = 0;
		ret = select(sock+1, &readfd, NULL, NULL, &timeout);
		switch(ret)
		{
			case -1:
				perror("select error");
				break;
			case 0:
				printf("select time up\n");
				break;
			default:
				if( FD_ISSET( sock, &readfd ) ){
					int from_len = sizeof( struct sockaddr_in);
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
					printf( "Recv msg is %s\n", buff );

					// 只有 数据报套接字 有这个时间戳  SOCK_DGRAM
					struct timeval tv = {0,0} ;
					errno = 0 ;
					ret = ioctl(sock , SIOCGSTAMP, &tv );
					if( ret == 0 ){
						printf("last read/recv package timestamp %ld %ld\n", tv.tv_sec , tv.tv_usec);
					}else{
						// SIOCGSTAMP error -1 2 No such file or directory
						printf("SIOCGSTAMP error %d %d %s \n", ret , errno,strerror(errno));
					}

				}
		}

		sleep(10);

	}
	close(sock);
	return 0 ;
}
