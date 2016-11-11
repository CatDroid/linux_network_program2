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


int main(int argc ,char**argv)
{
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


	int snd_size = 512; // 不能设置程512 最小4608
	int optlen = sizeof(snd_size);
	ret = setsockopt(server, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(ret){ 			// 会传递给client的socket 不能少于 4608
		printf("设置发送缓冲区大小错误\n");
	}
	snd_size = 0 ;
	optlen = sizeof(snd_size);
	ret = getsockopt(server, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
	if(ret){
		printf("获取发送缓冲区大小错误\n");
	}else{
		printf("snd buffer = %d\n" , snd_size );
	}

#define BACKLOG 5
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

	int optval = 1 ;
	ret = setsockopt(sock_client, SOL_SOCKET,SO_KEEPALIVE, &optval, sizeof(optval));
	if(ret){
		printf("SO_KEEPALIVE error %d %s \n",errno,strerror(errno));
		return -1 ;
	}


//	int alive_time = 60 ;
//	ret = setsockopt(sock_client, IPPROTO_TCP, TCP_KEEPALIVE, &alive_time, sizeof(alive_time));
//	if(ret){
//		printf("设置接收缓冲区大小错误\n");
//	}




	int keepIdle = 60;    		// 如该连接在60秒内没有任何数据往来,则进行此TCP层的探测
	int keepInterval = 5; 		// 探测发包间隔为5秒
	int keepCount = 3;        	// 尝试探测的次数.如果第1次探测包就收到响应了,则后2次的不再发

	ret = setsockopt(sock_client, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
	printf("TCP_KEEPIDLE %d %d %s\n" , ret ,  errno,strerror(errno));
	ret = setsockopt(sock_client, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
	printf("TCP_KEEPINTVL %d %d %s\n" , ret ,  errno,strerror(errno));
	ret = setsockopt(sock_client, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));
	printf("TCP_KEEPCNT %d %d %s\n" , ret ,  errno,strerror(errno));
	/*
	 * 需要双方都不发送数据
	 * 159	248.609709000	192.168.43.120	192.168.43.122	TCP	66	[TCP Keep-Alive] 10011 > 52805 [ACK] Seq=401 Ack=1 Win=227 Len=0 TSval=1563136 TSecr=2950756
	 * 160	248.725370000	192.168.43.122	192.168.43.120	TCP	66	[TCP Keep-Alive ACK] 52805 > 10011 [ACK] Seq=1 Ack=402 Win=279 Len=0 TSval=2962799 TSecr=1548101
	 *
	 * This is a TCP keep-alive segment
	 *
	 * 如果对方没有回复 keep-alive 指定次数
	 * 就会导致
	 * write -1 110 Connection timed out
	 * 已经建立的ESTABLIESD的socket没有了(netstat )
	 * */
	snd_size = 0 ;
	optlen = sizeof(snd_size);
	ret = getsockopt(sock_client, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
	if(ret){
		printf("获取发送缓冲区大小错误\n");
	}else{
		printf("snd buffer = %d\n" , snd_size );
	}




	printf("accpet client !\n");
	int max = 200;
#define DATA_LEN 200
	unsigned char WELCOME[DATA_LEN] = "welcome to china";
	WELCOME[DATA_LEN -3] = 'e';
	WELCOME[DATA_LEN -2] = 'n';
	WELCOME[DATA_LEN -1] = 'd';

	optval = 1 ;
	ret = ioctl(sock_client, FIONBIO , &optval);
	if(ret < 0){
		printf("non-block fail\n");
	}else{
		printf("non-block done\n");
	}
	while( max -- ){
		/*
		 * 	如果对方wifi突然断开了 没有告别TCP四次握手
		 * 	就会导致发送buffer满了 最后 write阻塞 或者 write返回 EAGAIN 11 Try again Resource temporarily unavailable
		 * 	这时候 协议栈发送缓冲区 其实 并没有满了
		 *
		 * 	write -1 11 Resource temporarily unavailable
			sleep 187
			write -1 11 Resource temporarily unavailable
			sleep 186
		 *
		 *  如果后面对方恢复网络
		 *  write -1 104 Connection reset by peer
		 *
		 *  实际对应已经很早就
		 *  read -1 110 Connection timed out
		 * */
		ret = write(sock_client ,WELCOME ,sizeof(WELCOME) );
		printf("write %d %d %s\n" ,ret , errno ,strerror(errno));
		printf("sleep %d\n",max);

		printf("sleep 120 enter \n");
		sleep(120); // 这样只要对方也不发送 就会产生 TCP ALIVE探测报文
		printf("sleep 120 leave \n");
	}
	printf("leave\n");

	close(sock_client);
	close(server);
}
