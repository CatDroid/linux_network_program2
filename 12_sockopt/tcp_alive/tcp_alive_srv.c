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

/* 配合APK netsocket  jni_netsocket_mytcpp.cpp来测试
 *
 * 	当服务器发送探测报文时，客户端可能处于4种不同的情况：仍然正常运行、已经崩溃、已经崩溃并重启了、
	由于中间链路问题不可达。在不同的情况下，服务器会得到不一样的反馈。

	(1) 客户主机依然正常运行，并且从服务器端可达
		客户端的TCP响应正常，从而服务器端知道对方是正常的。保活定时器会在两小时以后继续触发。

	(2) 客户主机已经崩溃，并且关闭或者正在重新启动(全部探测包都没有回复)
		客户端的TCP没有响应，服务器没有收到对探测包的响应，此后每隔75s发送探测报文，一共发送9次。
		socket函数会返回-1，errno设置为ETIMEDOUT，表示连接超时。

	(3) 客户主机已经崩溃，并且重新启动了(对方回复RST报文)
		客户端的TCP发送RST，服务器端收到后关闭此连接。
		socket函数会返回-1，errno设置为ECONNRESET，表示连接被对端复位了。

	(4) 客户主机依然正常运行，但是从服务器不可达
		双方的反应和第二种是一样的，因为服务器不能区分对端异常与中间链路异常。
		socket函数会返回-1，errno设置为EHOSTUNREACH，表示对端不可达。

		TCP是如何发送Keepalive探测报文的？ 分两种情况。
		1. 有新的数据段可供发送，且对端接收窗口还没被塞满。发送新的数据段，来作为探测包。
		2. 没有新的数据段可供发送，或者对端的接收窗口满了。发送序号为snd_una - 1、长度为0的ACK包作为探测包。


 */

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
		 *  实际对方可能已经断开
		 *  read -1 110 Connection timed out
		 *  也对方可能还在read协议栈原来的东西，但是断网重新连接wifi的话,
		 *  发送方可能继续发送原来协议栈的内容, 重传原来的PUSH+ACK+数据 但是
		 *	就会收到来自对方的 'RST报文', 结果导致了 Connection reset by peer
		 *  117	624.548497000	192.168.43.122	192.168.43.120	TCP	54	52820 > 10011 [RST] Seq=1 Win=0 Len=0
		 * */
		ret = write(sock_client ,WELCOME ,sizeof(WELCOME) );
		printf("write %d %d %s\n" ,ret , errno ,strerror(errno));
		printf("sleep %d\n",max);

		sleep(5);
		//printf("sleep 120 enter \n");
		//sleep(120);
		//printf("sleep 120 leave \n");
		/*
		 * 这样只要对方也不发送 就会产生 TCP ALIVE探测报文
		 * a.如果对方不能回复TCP ALIVE 那么本地这个已经ESTABLISH的TCP socket就会关闭
		 * 后面write就会错误 write -1 110 Connection timed out
		 * b.如果对方还在网络上 而且还能回复ACK TCP ALIVE 那么就不会关闭socket
		 *
		 * c.需要双方都不发送数据,才会发送KEEP ALIVE探测报文(This is a TCP keep-alive segment)
		 * (发送 	:KEEP ALIVE探测报文)
		 * 159	248.609709000	192.168.43.120	192.168.43.122	TCP	66	[TCP Keep-Alive] 10011 > 52805 [ACK] Seq=401 Ack=1 Win=227 Len=0 TSval=1563136 TSecr=2950756
		 * (回复ACK 	:KEEP ALIVE探测报文 )
		 * 160	248.725370000	192.168.43.122	192.168.43.120	TCP	66	[TCP Keep-Alive ACK] 52805 > 10011 [ACK] Seq=1 Ack=402 Win=279 Len=0 TSval=2962799 TSecr=1548101
		 *
		 * 如果对方没有ACK keep-alive 指定次数
		 * 就会导致
		 * 本地 write -1 110 Connection timed out
		 * 已经建立的ESTABLIESD的socket没有了(netstat )
		 *
		 * d. 不会导致本地发送TCP Alive 报文: ??
		 * 	 	对方突然断网了 无法完成路由 不能路由到对方主机
		 *
		 * e.实际上 在对方不断网的时候, 本地发送协议栈满了的时候,也会发送TCP Alive报文(ACK报,而不是PUSH报) 这时候远端也会返回(ACK报)
		 *		这个TCP Alive报文实际用来告诉对方'发送缓冲区'已经满了(TCP Full Window)
		 *
		 * e.在对方突然断网之后,对段还是可以write数据,但是TCP协议栈会出现‘TCP重传’(TCP Retransmission)
		 * 	 重传8次之后 就会不会再重传了
		 *	 9	20.000461000	192.168.43.120	192.168.43.122	TCP	266	10011 > 52820 [PSH, ACK] Seq=801 Ack=1 Win=227 Len=200 TSval=925808 TSecr=4171120
		 *	 10	20.248332000	192.168.43.120	192.168.43.122	TCP	266	[TCP Retransmission] 10011 > 52820 [PSH, ACK] Seq=801 Ack=1 Win=227 Len=200 TSval=925870 TSecr=4171120
		 *	超过'重传次数' 就会认为连接异常,关闭链接 这时候write出错 返回 113 No route to host
		 *  #define	ECONNABORTED	103	 // Software caused connection abort 连接中止
		 *  这个时间在ubuntu上大概是15分钟
		 */
	}
	printf("leave\n");

	close(sock_client);
	close(server);
}
