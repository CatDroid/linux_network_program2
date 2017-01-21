
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>		// fcntl F_SETFL O_NONBLOCK
#include <sys/select.h>	// FD_SET FD_ZERO fd_set


#include <sys/socket.h> // NDK中的确有sys/socket定义了所有socket操作函数 AF_INET socket.h SOCK_STREAM socket_type.h
#include <netinet/in.h>	// INADDR_ANY
#include <netinet/tcp.h> // TCP_INFO
#include <sys/ioctl.h>	// FIONREAD

#define SERVER_PORT 8899
//#define TARGET_IP "127.0.0.1"
#define TARGET_IP "192.168.43.120"
#define LOCAL_PORT 19999

unsigned int dumpTCPstate(int socket , const char* stage )
{
	struct tcp_info optval;
	int tcp_info_len = sizeof(struct tcp_info);
	int ret=  getsockopt(socket, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
	printf("[%s]connect status is %d \n" , (stage!=NULL)?stage:"unknown" , optval.tcpi_state);
	return optval.tcpi_state;
}

//int pipefd[2];

static void signal_handler(int signo) {
	switch(signo){
	case SIGINT:
		//write(pipefd[1],"1" ,1);
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

	int ret = 0 ;

	//pipe(pipefd);
	int client;
	client = socket(AF_INET, SOCK_STREAM, 0);
	if (client == -1) {
		printf("socket error %d %s \n", errno ,strerror(errno));
		return -1;
	} else {
		printf("socket success\n");
	}

	socklen_t snd_size = 4096;
	int optlen = sizeof(snd_size);
	ret = setsockopt(client, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
	if(ret < 0 ){ 			// 会传递给client的socket 不能少于 4608
		printf("设置发送缓冲区大小错误 %d %s \n",errno ,strerror(errno));
	}

	struct sockaddr_in targetaddr;
	bzero(&targetaddr, sizeof(struct sockaddr_in));
	targetaddr.sin_family = AF_INET;
	targetaddr.sin_port = htons(SERVER_PORT);
	targetaddr.sin_addr.s_addr = inet_addr(TARGET_IP);

	dumpTCPstate(client,"before connect");

	if (!connect(client, (struct sockaddr *) &targetaddr, sizeof(struct sockaddr_in))) {
		printf("connect ok\n");
	} else {
		printf("connect error %d %s \n" ,errno ,strerror(errno));
		return -1 ;
	}

	fcntl(client, F_SETFL, O_NONBLOCK);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT, signal_handler);

	char rbuf[256];
#define WRI_LEN 255
	char wbuf[WRI_LEN ];
	int j = 0 ;
	for( ; j < WRI_LEN; j++){
		wbuf[j] = 'a' + j % 26 ;
	}
	int isshutdown = 0 ;
	while (1) {
		int maxfd = -1 ;
		fd_set readfds; FD_ZERO(&readfds);
		fd_set writefds; FD_ZERO(&writefds);
		FD_SET( client, &readfds);if(client > maxfd ) maxfd = client ;
		//FD_SET( pipefd[0], &readfds);if(pipefd[0] > maxfd ) maxfd = pipefd[0] ;
		if(!isshutdown) FD_SET( client , &writefds);if(client > maxfd ) maxfd = client ;
		int change = 0 ;
		if (change = select(maxfd + 1, &readfds, &writefds, NULL, NULL), change > 0) {
			if (FD_ISSET( client , &readfds)) {
				ret = read(client, rbuf , sizeof(rbuf) - 1 );
				if( ret == 0 ){
					printf("peer socket close \n");
					break;
				}else{
					rbuf[ret] = '\0' ;
					printf("read from server %s \n" , rbuf);
				}
				change--;
			}

			if( FD_ISSET(client , &writefds) ){
				change--;
				ret = write(client,wbuf,WRI_LEN);
				if(ret < 0){
					// EPIPE ECONNRST
					printf("write error %d %s\n",errno ,strerror(errno));
					break;
				}else{
					printf("write %d of %d \n", ret , WRI_LEN );
				}

			}
			if( change != 0 ){
				printf("change? what happen??\n");
			}
		}else if (change == -1) {
			printf("select error:%d,%s\n", errno, strerror(errno));
			if (errno == EINTR) {
				printf("select EINTR shutdown write! \n");
				break;
				// 1.如果调用了close之后 还使用这个描述符 会导致:
				// select error:9,Bad file descriptor
				//
				// 2.另外 如果调用了close 本地处于FIN_WAIT1 远端还处于ESTABLISH状态读取缓存数据
				// 由于本地已经close调用 关闭了读 所以远端这时候write 就会导致本地发送RST报文
				// socket立刻关闭 协议栈还没有发送完的数据就没了
				// 远端write还是成功,但是read完了自己协议栈buffer之后
				// 就会出现错误 Connection reset by peer
//				close(client);
//				sleep(120);
//				printf("sleep done \n");
//				break;

				/*
				 * close 与 shutdown 区别
				 *	1. 关闭本进程的socket id 但链接还是开着的 用这个socket id的其它进程还能用这个链接 能读或写这个socket id
				 *	2. (shutdown SHUT_WR)破坏了socket链接，本地读的时候可能侦探到EOF结束符，本地写的时候可能会收到一个SIGPIPE信号
				 *		所以引用这个连接的文件描述符 都不能进行写
				 *	3. 即使调用shutdown(fd, SHUT_RDWR)也不会关闭fd，最终还需close(fd)
				 * */
				isshutdown = 1 ;

				shutdown(client,SHUT_WR);
				/*
				 * 本地 49571 远端 8899
				 *
				 * 1. 本地shutdown之后 (还把socket加入select writefds)
				 * 如果调用了shutdown ,本地socket处于FIN_WAIT1 这时候还write就会导致 EPIPE
				 * 当调用了shutdown之后,如果继续调用select writefds包含已经shutdown的socket
				 * 结果select还是能够返回writefds是有效的,所以shutdown之后要把socket从writefds移除
				 *
				 *  select EINTR shutdown write!
					write error 32 Broken pipe
				 */

				/*
				 *  2. 本地shutdown之后 (不在对socket加入select writefds)
				 *  本地是FIN_WAIT1 远端还是ESTABLISHED
				 *  这时候
				 *  本地FIN_WAIT1不能发送数据
				 *  但是远端(ESTABLISHED没有FIN-ACK)还是可以发送数据
				 *  本地如果select readfs还是可以读取数据(对于本地没有关闭读取 只是shutdown SHUT_WR)
				 * 	tcp        0      0 0.0.0.0:8899       0.0.0.0:*      LISTEN
					tcp        0   3810 127.0.0.1:49571  127.0.0.1:8899   FIN_WAIT1
					tcp     1036      0 127.0.0.1:8899   127.0.0.1:49571  ESTABLISHED
				 *
				 *	当远端协议栈已经接收完毕后(但是用户还没有read完毕)
				 *	本地FIN_WAIT2 远端就 CLOSE_WAIT
				 *	tcp        0      0 127.0.0.1:49571  127.0.0.1:8899    FIN_WAIT2
					tcp      997      0 127.0.0.1:8899   127.0.0.1:49571   CLOSE_WAIT
					tcp        0      0 0.0.0.0:8899     0.0.0.0:*         LISTEN

					当远端用户read完毕了 调用了close之后
					tcp        0      0 0.0.0.0:8899     0.0.0.0:*         LISTEN
					tcp        0      0 127.0.0.1:49571  127.0.0.1:8899    TIME_WAIT

					总结:所以write错误后,要read到结束,才能把协议栈缓冲的数据读取完毕

						如果一方调用了close关闭读端/写端 一方处于FIN_WAIT1状态  对方write就会导一方完全close socket并发送RST报文
						如果一方只是调用shutdown SHUT_WR关闭写端 但是读端还有效
													一方处于FIN_WAIT1状态 对方还是可以write的 直到协议栈的buffer接受完毕
													对方read返回0之后 close掉socket
													这样一方也从read返回0 close掉socket
				 */



			}
		} else if (change == 0) {
			printf("select timeout\n");
		}
	}

	/*
	 * 1. a.只要TCP栈的读缓冲里还有未读取（read）数据，则调用close时会直接向对端发送RST,并且双方连接立刻CLOSE状态(netstat已经看不到)
	 * 		 但是对段socket还是可以把协议栈的数据读取完毕
	 * 	  b.如果没有读数据 而调用了close 只会发送FIN 并且自己处于FIN_WAIT1状态
	 *
	 * 2. SO_LINGER与close，当SO_LINGER选项开启但超时值为0时，调用close直接发送RST
	 * 		（这样可以避免进入TIME_WAIT状态，但破坏了TCP协议的正常工作方式），SO_LINGER对shutdown无影响
	 *
	 * 3. TCP连接上出现RST与随后可能的TIME_WAIT状态没有直接关系，主动发FIN包方必然会进入TIME_WAIT状态，
	 * 		除非不发送FIN而直接以发送RST结束连接(就是还有读数据 但是没有读完 就立刻close)
	 */
	close(client);
	return 0;
}
