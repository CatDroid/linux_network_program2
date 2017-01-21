
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

int exitpipe[2];

static void sigint_handler(int signo){
	//printf("sigint_handler = %d\n" , signo );
	//printf("SIGINT = %d\n" , SIGINT );
	ssize_t ret = write(exitpipe[1],"1",strlen("1")+1);
	printf("[sigint_handler]ret = %zd  errno = %d err = %s \n" , ret , errno , strerror(errno));
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
	// 本网络接口发送出去的广播 本网络接口是不能接收到的
	// 但是lo本地回环这个网络接口是可以获取到的
	// 所以如果在同一个主机上面测试 服务端不能绑定到与客户端一样的网络接口
	// wrieshark要监听lo和usb0两个网络接口

	struct sockaddr_in local_addr;
	memset((void*)&local_addr, 0, sizeof(struct sockaddr_in));
#ifdef BIND_DEVICE
	memcpy(&local_addr, &ifr.ifr_broadaddr, sizeof(struct sockaddr_in ));
#else
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// 对本地任何一个网卡
#endif
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(MCAST_PORT);		// 的MCAST_PORT端口 的广播 或者其他UDP数据报

	ret = bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));
	if(ret != 0){
		perror("bind error");
		return -1 ;
	}
	printf("server bind to %s %d\n", inet_ntoa(local_addr.sin_addr), MCAST_PORT);

	unsigned char exitflag = 0 ;
	ret = pipe(exitpipe);
	if(ret != 0){
		perror("pipe error");
		return -1 ;
	}
	// write(exitpipe[1],"1",strlen("1")+1); // 管道没有读的话 还是会缓存在里面

	signal(SIGINT, sigint_handler);

	fcntl(sock,F_SETFL, O_NONBLOCK);

	struct sockaddr_in from_addr;
	//memset(&from_addr, 0 , sizeof(struct sockaddr_in) );

   	int max_fd = -1 ;
	fd_set readfd;
	int count = -1;
#define BUFFER_LEN 32
	char buff[BUFFER_LEN];
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	while(!exitflag)
	{
		FD_ZERO(&readfd);
		FD_SET(sock, &readfd); if( sock > max_fd ) max_fd = sock;
		FD_SET(exitpipe[0], &readfd); if( exitpipe[0] > max_fd ) max_fd = exitpipe[0] ;
		//ret = select(sock+1, &readfd, NULL, NULL, &timeout);
		//printf("max_fd %d sock %d exitpipe[0] %d\n" , max_fd ,sock,exitpipe[0]);
		ret = select(max_fd + 1, &readfd, NULL, NULL, NULL);
		switch(ret){
			case -1:
				printf("select error:%d,%s\n" , errno , strerror(errno));
				// 如果按下Ctrl+C select会返回EINTR Interrupted system call
				if(errno == EINTR){
					exitflag = 1 ;
				}
				break;
			case 0:
				printf("timeout\n");
				break;
			default:
				if( FD_ISSET( sock, &readfd ) ){

					int from_len =  sizeof(struct sockaddr_in) ;
					// 必须设置这个参数 否则 recefrom返回 地址信息错误
					count = recvfrom( sock, buff, BUFFER_LEN, MSG_TRUNC,
										(struct sockaddr*) &from_addr, &from_len );
					// recefrom会拿到客户端的IP地址信息struct sockaddr_in
					// 即使客户端发送的是广播
					if( count > BUFFER_LEN){
						printf("消息过大 已被截断\n");
						buff[BUFFER_LEN - 1 ] = '\0';
					}else if(count == BUFFER_LEN) {
						buff[BUFFER_LEN - 1 ] = '\0';
					}else{
						buff[count] = '\0' ;
					}

					printf( "Recv msg is %s\n", buff );

//					struct ifreq ifr_device; // 必须要给定一个index 才能获得设备名字
//					if(ioctl(sock,SIOCGIFNAME,&ifr_device) == -1){
//						perror("ioctl error"); // ioctl error: No such device 如果没有对应网络接口
//						return -1 ;
//					}

					if( strstr( buff, IP_FOUND ) ){
						printf("reply %s to %s\n" , IP_FOUND_ACK , inet_ntoa(from_addr.sin_addr) );
						memcpy(buff, IP_FOUND_ACK, strlen(IP_FOUND_ACK)+1);
						count = sendto( sock, buff, strlen( buff ),0,( struct sockaddr*) &from_addr, from_len );
						if(count < 0){
							perror("sendto error");
						}
					}// sendto也会告诉客户端,服务端的IP地址信息
				}

				if( FD_ISSET(exitpipe[0], &readfd) ){
					count = read(exitpipe[0] , buff , BUFFER_LEN-1);
					buff[count] = '\0';
					printf("exit count = %d buff = %s\n" , count , buff );
					exitflag = 1 ;
				}
		}
	}

	close(exitpipe[0]);
	close(exitpipe[1]);
	close(sock);
	return 0 ;
}
