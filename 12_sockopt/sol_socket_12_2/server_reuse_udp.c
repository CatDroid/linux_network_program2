/*
 * server_reuse.c
 *
 *  Created on: 2016年11月5日
 *      Author: hanlon
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>		// signal
#include <fcntl.h>		// fcntl F_SETFL O_NONBLOCK
#include <sys/select.h>	// FD_SET FD_ZERO fd_set

#include <sys/socket.h> // NDK中的确有sys/socket定义了所有socket操作函数 AF_INET socket.h SOCK_STREAM socket_type.h
//#include <linux/in.h>	// INADDR_ANY  用这个会冲突 !!! 跟  arpa/inet.h
#include <netinet/in.h>	// 	INADDR_ANY socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <arpa/inet.h>	// 	inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
						//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
						//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现


#define WELCOME "welcome to server 2016\n"
#define REFUSED "refuse\n"
#define LOCAL_PORT 18899

static void signal_handler(int signo) {
	switch(signo){
	case SIGINT:
		printf("catch SIGINT\n");
		break;
	case SIGIO:
		printf("catch SIGIO\n");
		break;
	default:
		printf("catch unknown signal %d !\n", signo);
		break;
	}
	return;
}



int main(int argc, char**argv) {

	int ret = 0;
	int server = -1;
	int pid = getpid();

	server = socket(AF_INET, SOCK_DGRAM, 0);

	/*
	 *
	 * 只要每個UDP socket都有設定SO_REUSERADDR，則可以綁定一模一樣的IP跟port
	 *
	 * 发送:
	 * 当两个或多个UDP套接字绑定了同一个IP和端口后，每一个套接字都可以独立进行数据的发送
	 *
	 * multicast：
	 * 如果多個UDP綁定同一個IP與port，當有UDP封包送到該IP:port時
	 * 每一個socket都會收到一份相同的封包
	 * 也就是, 如果该数据报的目的地址是广播或者多播，则每个套接口都会收到该数据报的拷贝
	 *
	 * unicast：(对于UDP socket，内核尝试平均的转发数据报)
	 * 如果多個socket綁定同樣的IP跟port
	 * connect到封包來源IP:port的connected UDP socket可以優先接收
	 * 如果有多個socket滿足這個條件，則最晚bind的connected UDP socket可以收到封包
	 * 如果沒有connected UDP socket，則最晚bind的一般UDP socket可以收到封包
	 *
	 * 但如果在recvfrom时使用了MSG_PEEK参数，则数据报仍会留在缓冲中供其他套接字使用
	 * MSG_PEEK有助于多个套接字共享接收到的数据报，但仍需小心处理。
	 *
	 * netstat  -aun
		Active Internet connections (servers and established)
		Proto Recv-Q Send-Q Local Address           Foreign Address         State
		udp        0      0 0.0.0.0:8899            0.0.0.0:*
		udp        0      0 0.0.0.0:8899            0.0.0.0:*


	必须第一个进程 和 后续所有进程 都设置 SO_REUSEADDR 才能重复绑定端口
	 */
	int optval = 1 ;
	ret = setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(optval));
	if (ret < 0) {
		printf("setsockopt SO_REUSEADDR %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}


	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = ntohs(LOCAL_PORT);
	local.sin_addr.s_addr = htonl(INADDR_ANY);



	ret = bind(server, (struct sockaddr*) &local, sizeof(local));
	if (ret < 0) {
		printf("bind error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}


	if(argc == 2 ){ // 带参数的话 第二个作为客户端端口号
		/*
		 *
		关于connected udp套接字:
		如果 local bind INADDR_ANY
		而 remote connect INADDR_ANY  或者 192.168.42.129 两种情况下

		不进行connect
		udp        0      0 0.0.0.0:18899           0.0.0.0:*

		两种情况进行connect之后
		netstat  -aun
		Active Internet connections (servers and established)
		Proto Recv-Q Send-Q Local Address           Foreign Address         State
		udp        0      0 127.0.0.1:18899         127.0.0.1:47129         ESTABLISHED

		or 如果connect的时候 不指定目标IP 会被默认成127.0.0.1 这样对方发送到本机IP 这里就不能接收到了
		udp        0      0 192.168.42.82:18899     192.168.42.129:47129    ESTABLISHED <= 这种状态代表UDP使用了connect



		接收端的部分UDP socket為connected:
			指定destination為IP_A:Port_A
			只會收到來自IP_A:Port_A的封包，封包也只能傳送給IP_A:Port_A

		*/
		struct sockaddr_in target;
		target.sin_family = AF_INET;
		target.sin_port = ntohs(atoi(argv[1]));
		//target.sin_addr.s_addr = htonl(INADDR_ANY);
		target.sin_addr.s_addr = inet_addr("192.168.42.82");
		ret = connect(server, (struct sockaddr*)&target, sizeof(target));
		if(ret < 0){
			printf("connect fail %d %s\n", errno ,strerror(errno));
		}else{
			printf("server reuse udp  connect to port %d \n" , ntohs(target.sin_port) );
		}
	}else{
		printf("server reuse udp not connect to certern ip and port \n");
	}



	//signal(SIGINT, signal_handler);

	int exitflag = 0;
	#define BUFFER_LEN 16
	char rbuf[BUFFER_LEN];
	while (!exitflag)
	{
		struct sockaddr_in from_addr;
		int from_len = sizeof(struct sockaddr_in);// 不会因为EINTR返回
		int count = recvfrom( server, rbuf, BUFFER_LEN, MSG_TRUNC,
					( struct sockaddr*) &from_addr, &from_len );

		if( count > BUFFER_LEN){
			printf("消息过大 已被截断\n");
			rbuf[BUFFER_LEN - 1 ] = '\0';
		}else if(count == BUFFER_LEN) {
			rbuf[BUFFER_LEN - 1 ] = '\0';
		}else if(count < 0 ){
			printf("recvfrom error %d %s \n", errno, strerror(errno) );
			if(errno == EINTR ){
				printf("EINTR exit \n");
				exitflag = 1 ;
				break;
			}else{
				printf("error but continue\n");
				continue ;
			}
		}else if(count == 0 ){
			printf("recefrom ??? 无连接 也会read到0 ???\n");
		}else{
			rbuf[count] = '\0' ;
		}

		// 如果对端不存在的话 sendto 依旧可以成功 但是会返回ICMP
		// ICMP Type 3 Destination unreachable  不可到达类型
		// 		Code 3 Port unreachable			端口不可到达(具体原因)
		printf( "Recv msg is %s from %s %d \n", rbuf , inet_ntoa(from_addr.sin_addr) , ntohs(from_addr.sin_port) );
		count = sendto(server, "ok" ,sizeof("ok") , 0 ,( struct sockaddr*) &from_addr , from_len);
		if(count <= 0 ){
			printf("sendto error %d %s\n",errno,strerror(errno));
		}
	}


	close(server);

	return 0;

}
