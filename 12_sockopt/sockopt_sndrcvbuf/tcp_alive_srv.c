/*
 * tcp_alive_srv.c
 *
 *  Created on: 2016年11月9日
 *      Author: hanlon
 */


#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h> 		// FIONBIO
#include <signal.h>			// SIG_IGN

#include <sys/socket.h>		// socket
#include <netdb.h>			// getprotobyname gethostbyname
#include <netinet/in.h> 	// socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <netinet/ip_icmp.h>// struct icmp  ICMP_ECHO ICMP_ECHOREPLY
#include <netinet/ip.h>		// struct ip
#include <netinet/tcp.h>
#include <arpa/inet.h>		// inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
							//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
							//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现


#define SRV_PORT 10011
#define DATA_LEN 200
#define BACKLOG 5
// 配合APK netsocket  jni_netsocket_mytcpp.cpp来测试
int main(int argc ,char**argv)
{
	int snd_size = 512;
	int optlen = sizeof(snd_size);

	int ret = 0 ;
	int server = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = ntohs(SRV_PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(server, (struct sockaddr*) &local, sizeof(local));
	if (ret < 0) {
		printf("bind error %d %s\n", errno, strerror(errno));
		close(server);
		return -1 ;
	}

	snd_size = DATA_LEN ;
	optlen = sizeof(snd_size);
	ret = setsockopt(server, SOL_SOCKET, SO_SNDLOWAT,&snd_size, optlen);
	if(ret){
		/*
		 * #define	ENOPROTOOPT	92	// Protocol not available
		 * 来自man socket介绍
		 * 在linux上 SO_SNDLOWAT 是不可变的 setsockopt会返回错误 92
		 * 			SO_RCVLOWAT 是可变的 从linux2.4以后
		 * 								当前实现 select和poll是不管SO_RCVLOWAT的 只要有一个字节有效就返回
		 * 								返回后在后续的read中会阻塞直到SO_RCVLOWAT字节有效
		 *
		 * 	TCP_NOTSENT_LOWAT 是发送协议栈的最大值要低于TCP_NOTSENT_LOWAT
		 * 	SO_SNDLOWAT 是 可用发送空间至少达到SNDLOWAT时候
		 */
		printf("设置发送缓冲区下限错误 %d %s \n" ,errno,strerror(errno));
	}else{
		printf("设置发送缓冲区下限 = %d\n" , snd_size );
	}
	snd_size = 0 ;
	optlen = sizeof(snd_size);
	ret = getsockopt(server, SOL_SOCKET, SO_SNDLOWAT,&snd_size, &optlen);
	if(ret){
		printf("获取发送缓冲区下限错误 %d %s \n" ,errno,strerror(errno));
	}else{
		// ubuntu 发送缓冲区  默认是 1
		printf("获取发送缓冲区下限 = %d\n" , snd_size );
	}


	snd_size = 512; 	// 不能设置程512 最小4608
	optlen = sizeof(snd_size);
	ret = setsockopt(server, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(ret < 0 ){ 			// 会传递给client的socket 不能少于 4608
		printf("设置发送缓冲区大小错误 %d %s \n",errno ,strerror(errno));
	}
	snd_size = 0 ;
	optlen = sizeof(snd_size);
	ret = getsockopt(server, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
	if(ret <0 ){
		printf("获取发送缓冲区大小错误 %d %s \n",errno ,strerror(errno));
	}else{
		printf("server snd buffer = %d\n" , snd_size );
	}


//	snd_size = 0 ;
//	optlen = sizeof(snd_size);
//	ret = getsockopt(server, IPPROTO_TCP, TCP_MAXRT,&snd_size, &optlen);
//	if(ret){
//		printf("获取最大重传时间错误\n");
//	}else{
//		printf("snd buffer = %d\n" , snd_size );
//	}

	ret = listen(server, BACKLOG);
	if (ret < 0) {
		printf("listen error %d %s\n", errno, strerror(errno));
		close(server);
		return -1 ;
	}


	printf("wait for client !\n");
	struct sockaddr_in client_addr ;
	unsigned int len = sizeof(struct sockaddr_in);
	int sock_client = accept(server,(struct sockaddr*)&client_addr, &len);
	if (sock_client < 0 ) {
		printf("accept error %d %s\n",errno,strerror(errno));
		return -1 ;
	}

	printf("client incoming !\n");

	snd_size = 0 ;
	optlen = sizeof(snd_size);
	ret = getsockopt(sock_client, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
	if(ret){
		printf("获取发送缓冲区大小错误\n");
	}else{
		printf("发送缓冲区大小 = %d\n" , snd_size );
	}


	printf("accpet client !\n");
	int max = 500;

	unsigned char WELCOME[DATA_LEN] ;
	int j = 0;
	for( ; j < DATA_LEN ; j++ ){
		WELCOME[j] = 'a' + j % 26 ;
	}
	WELCOME[DATA_LEN -3] = 'e';
	WELCOME[DATA_LEN -2] = 'n';
	WELCOME[DATA_LEN -1] = 'd';

	int optval = 1 ;
	ret = ioctl(sock_client, FIONBIO , &optval);
	if(ret < 0){
		printf("non-block fail\n");
	}else{
		printf("non-block done\n");
	}

	signal(SIGPIPE,SIG_IGN);

	while( max -- ){
		errno = 0 ;
		struct tcp_info optval;
		int tcp_info_len = sizeof(struct tcp_info);
		ret =  getsockopt(sock_client, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
		printf("optval.tcpi_state  = %d \n" , optval.tcpi_state);

		ret = write(sock_client ,WELCOME ,sizeof(WELCOME) );
		if(errno == EPIPE){
			printf("EPIPE\n"); 	// 如果远端的接收buffer满的情况下(ACK报文 TCP Zero Window) 连FIN都不会发 直接发送FIN报文
								// 如果远端发送FIN 本地回复ACK后 socket处于CLOSE_WAIT
								// CLOSE_WAIT状态下 还是可以write 发送协议站 也会发送出去
								// 但是这是会收到远端 RST报文  本地端的socket就会变成 CLOSE
			break;				// 这时候如果write的话 就会导致 EPIPE SIGPIPE
		}
		if(ret != sizeof(WELCOME)){
			/*
			 * 在发送协议栈满的情况下 有可能这里只写了一部分 !
			 */
			printf("----------------------------- write not enough %d " ,ret );

		}
		printf("write %d %d %s\n" ,ret , errno ,strerror(errno));
		printf("sleep %d\n",max);
		sleep(5); // 客户端间隔20秒接收一次 服务端间隔5秒发送一次


		/*
		 * 1. 如果发送的速度 比 接收的速度 要快
		 * 		那么 最后导致  接收的缓冲和发送的缓冲都满了 发送socket如果是non-block返回EAGAIN(Resource temporarily unavailable)
		 *
		 * 2. 接收端 在接收到一定的时候，接收buffer从满 减低到 一定程度(??有一半空间??)
		 * 		那么 会发送 window update报文  给到 发送端 ,
		 * 		发送协议栈就会减少(发送协议栈的数据传给了接收端)
		 * 		结果 发送端就可以write成功
		 *
		 * 3. 在发送协议栈满的情况下
		 * 		发送方会不断发送ACK报文 通知自己的空间已经满了(Full window)
		 * 		接收方会应答这个ACK报文 告诉发送方自己的接收空间可能也已经满了/没有剩余空间(Zero window)
		 *		当接收方有接收空间的时候 会发送ACK报(TCP Window Update)
		 *
		 * 4. wirteshark中
		 * 		TCP Alive			只发送ACK 没有数据
		 * 		TCP FullWindow 		发送PUSH+ACK 但是window size是满了(发送方)
		 * 		TCP ZeroWindow		发送ACK 但是window size是空了(接收方)
		 * 		TCP Window Update	发送ACK 但是window size变小了(接收方通知 发送方有空间了)
		 * 		TCP Retransmission  重传PUSH+ACK
		 *
		 * 		SEQ	不发送数据 保持上一个接收到的 SEG的发送值 ,比如KEEP ALIVE报文
		 * 			如果发送数据 就应该是上一个接收到的ACK的值,比如发送KEEP ALIVE报文 然后收到ALIVE报文ACK,然后发送数据 PUSH+ACK 就应该是收到ALIVE ACK报文的ACK值
		 * 		ACK 在接收的SEQ加上数据长度恢复 没有数据(长度)就只加一
		 */
	}
	printf("leave\n");

	close(sock_client);
	close(server);
}
