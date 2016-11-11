
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

#define REUSE_PORT 8899
#define REFUSED "refuse\n"
#define TARGET_IP "127.0.0.1"
#define URG_CMD "urg"
#define SEND_URG_OOB_DATA "ABCDEFGHIJKLMN"

#define LOCAL_PORT 19999

int main(int argc, char**argv) {
	char rbuf[256];
	char wbuf[255];
	int client;
	struct sockaddr_in targetaddr;
	int reSend;
	int ret = 0 ;
	client = socket(AF_INET, SOCK_STREAM, 0);
	if (client == -1) {
		printf("socket error %d %s \n", errno ,strerror(errno));
		return -1;
	} else {
		printf("socket success\n");
	}


	/*
	 * 非监听TCP socket
	 * 不使用SO_REUSEADDR 不能bind同样的地址
	 * 使用SO_REUSEADDR   可以bind通配地址和特定地址
	 * 					但是这种情况下,目标地址和端口不能完全一样
	 * 					否者错误:
	 * 					connect error 99 Cannot assign requested address
	 *
	 * bind的时候 在netstat 还不能看到创建了连接 只有在connect之后才有 !!
	 * connect的时候会根据目标地址 修改源地址  如果完全一样的话 就会 EREUSEADDRSS
	 * connect如果是通配地址 会被转成127.0.0.1的 这个需要注意
	*/
//	int optval = 1 ;
//	ret = setsockopt(client,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(optval));
//	if (ret < 0) {
//		printf("setsockopt SO_REUSEADDR %d %s\n", errno, strerror(errno));
//		close(client);
//		return -1;
//	}
//	struct sockaddr_in local;
//	local.sin_family = AF_INET;
//	local.sin_port = ntohs(LOCAL_PORT);
//	local.sin_addr.s_addr = htonl(INADDR_ANY);
//	ret = bind(client, (struct sockaddr*) &local, sizeof(local));
//	if (ret < 0) {
//		printf("bind error %d %s\n", errno, strerror(errno));
//		close(client);
//		return -1;
//	}


	// sleep(20);


	bzero(&targetaddr, sizeof(struct sockaddr_in));
	targetaddr.sin_family = AF_INET;
	targetaddr.sin_port = htons(REUSE_PORT);
	targetaddr.sin_addr.s_addr = inet_addr(TARGET_IP);

	{
		struct tcp_info optval;
		int tcp_info_len = sizeof(struct tcp_info);
		int ret=  getsockopt(client, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
		printf("before connect status is %d \n" , optval.tcpi_state);
	}
	if (!connect(client, (struct sockaddr *) &targetaddr, sizeof(struct sockaddr_in))) {
		printf("connect ok\n");
		bzero(rbuf, sizeof(rbuf));
		if (read(client, rbuf, sizeof(rbuf)) > 0) {
			printf("%s\n", rbuf);
			if( !strncmp(rbuf , REFUSED ,strlen(REFUSED)) ){
				close(client);
				return 0;
			}
		}
	} else {
		printf("connect error %d %s \n" ,errno ,strerror(errno));
		return -1 ;
	}

	//fcntl(sock, F_SETFL, O_NONBLOCK);
	signal(SIGPIPE,SIG_IGN); // 这样防止了SIGPIPE 默认处理程序 杀掉进程  write返回EPIPE

	while (1) {
		bzero(wbuf, sizeof(wbuf));
		printf("please enter information:\n");
		fgets(wbuf, sizeof(wbuf), stdin);// 会包含\n strlen包含最后一个\n
		printf("read user %zd %zd last is \\n ? %s \n" , sizeof(wbuf) , strlen(wbuf) ,
						( wbuf[strlen(wbuf)-1] == '\n')?"true":"false" );
		reSend = 0;
		{
			struct tcp_info optval;
			int tcp_info_len = sizeof(struct tcp_info);
			ret =  getsockopt(client, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
			printf("before write TCP state = %d\n" , optval.tcpi_state);

			if(optval.tcpi_state == TCP_CLOSE_WAIT){
				// 如果socket处于close_wait 那么read不会有异常 但是返回0(EOF) socket状态也不会改变
				printf("read in tcp_close wait\n");
				ret = read(client, rbuf, sizeof(rbuf) ) ;
				printf("read in tcp_close wait ret = %d %d %s \n",ret , errno ,strerror(errno));
				{
					struct tcp_info optval;
					int tcp_info_len = sizeof(struct tcp_info);
					ret =  getsockopt(client, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
					printf("after read TCP state = %d\n" , optval.tcpi_state);
				}
			}
		}

		/*
		 * TCP传输层:
		 * 通过TCP的紧急指针 发送带外数据
		 * 实际只能发送最后一个字节 作为 紧急数据
		 * 其他字节 作为 普通数据 发送
		 *
		 *  x x x x x x x x x a []
		 *                    | | "TCP紧急指针"设置成再下一个可用位置(下一次send缓冲数据的地方)
		 *					  |-带外字节
		 *
		 *	发送端TCP将为待发送的"下一个分节"在TCP首部中设置"URG标志"，
		 *	并把"紧急偏移字段"设置为指向带外字节之后的字节("TCP紧急指针")
		 *	不过该分节可能含也可能"不含我们标记的OOB的那个字节"
		 *	OOB字节是否发送取决于
		 *	a. 在套接字发送缓冲区中 先于它的字节数
		 *	b. TCP 准备发送给对端的分节大小
		 *	c. 以及 "对端通告的当前窗口"
		 *
		 *	TCP首部指出发送端已经进入紧急模式（即伴随紧急偏移的URG标志已经设置）
		 *	但是由紧急指针所指的实际数据字节却不一定随同送出。
		 *
		 *	事实上即使发送端TCP因流量控制而暂停发送数据
		 *	（接收端的套接字接收缓冲区已满，导致其TCP想发送端TCP通告了一个值为0 的窗口）
		 *	"紧急通知"照样"不伴随任何数据的发送"。
		 *	这也是应用进程使用TCP紧急模式（即带外数据）的一个原因：
		 *	即便数据的流动会因为TCP的"流量控制"而停止，"紧急通知却总是无障碍的发送到对端TCP"
		 *
		 *	发送端TCP往往发送多个含有URG标志且紧急指针指向同一个数据字节的分节（通常是在一小段时间内）
		 *	也就是接受端会收到"多个分节" 含有URG标记
		 *	但只有第一个到达的会通过 SIGURG信号 导致通知接收进程有新的带外数据到达
		 *
		 * UDP传输层:
		 * 几乎每个传输层都各自有不同的带外数据实现
		 * 而UDP作为一个极端的例子，没有实现带外数据
		 */
		if( ! strncmp(wbuf , URG_CMD, strlen(URG_CMD) )){
			ret = send(client,SEND_URG_OOB_DATA ,strlen(SEND_URG_OOB_DATA) ,MSG_OOB );
			if( ret > 0){
				printf("send oob data using TCP urg  OK\n");
			}else{
				printf("send oob data using TCP urg fail %d %d %s\n", ret , errno ,strerror(errno));
			}
			continue ;
		}
		reSendData: if (write(client, wbuf, strlen(wbuf)) <= 0) {
			perror("write error");
				// 服务端拒绝了 就会close掉socket --> 客户端处于CLOSE_WAIT 服务端处于FIN_WAIT2
				// 客户端read					 --> 客户端还是处于CLOSE_WAIT 立刻返回read=0 EOF 应该关闭端口
				// 客户端write  			     --> 客户端处于LAST_ACK-->CLOSE 服务端处于TIME_WAIT 但是write成功
				// 如果还write 				 --> 会导致SIGPIPE信号 杀掉进程 已经write返回EPIPE错误
			break;
		} else {

			struct tcp_info optval;
			int tcp_info_len = sizeof(struct tcp_info);
			ret =  getsockopt(client, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);

			if(optval.tcpi_state==TCP_CLOSE_WAIT){
				printf("write in tcp_close wait\n");
				do{
					ret = write(client, "write_in_tcp_close_wait", strlen("write_in_tcp_close_wait"));
				}while(ret > 0 );
				printf("write error in TCP_CLOSE_WAIT%d %s \n",errno,strerror(errno));
			}else  {
				printf("before read TCP state = %d\n" , optval.tcpi_state);
			}


			bzero(rbuf, sizeof(rbuf));

			int readed = 0;
			ioctl(client, FIONREAD , &readed);
			// malloc(readed) 未必对方就已经发送数据 所以这里很有可能是 readed = 0
			// read 即使是BLOCK/阻塞的 如果buffer是0的话 也是立刻返回
			// read 0 0 Success
			// ret = read(client, rbuf, readed);
			// printf("read %d %d %s\n",ret ,errno ,strerror(errno));

			printf("wait for server ack readed %d \n" , readed );
			if (ret = read(client, rbuf, sizeof(rbuf)), ret  > 0) {
				// 如果设置sock为NONBLOCK 这里会导致 Resource temporarily unavailable
				// 这样错误之后 如果立刻调用close socket 就会导致服务端read过程中断,返回错误码:104 Connection reset by peer
				if (!strncmp(rbuf, "ok", 2)) {
					printf("server recvice data\n");
					printf("ack %s\n", rbuf);
				} else {
					printf("server ack %s\n", rbuf);
					if (reSend < 3) {
						reSend++;
						printf("reSend 第%d次\n", reSend);
						goto reSendData;
					} else {
						printf("reSend Error\n");
						printf("This process is about to exit....\n");
						break;
					}
				}

				// 只有 数据报套接字 才能获取上一个数据报的网卡接收时间戳  SOCK_DGRAM
//				struct timeval tv = {0,0} ;
//				errno = 0 ;
//				ret = ioctl(client , SIOCGSTAMP, &tv );
//				if( ret == 0 ){
//					printf("last read/recv package timestamp %ld %ld\n", tv.tv_sec , tv.tv_usec);
//				}else{
//					// SIOCGSTAMP error -1 2 No such file or directory
//					printf("SIOCGSTAMP error %d %d %s \n", ret , errno,strerror(errno));
//				}

			} else if(ret == 0 ) {
				printf("EOF socket closed by server\n");
				break;
			}else {
				printf("read error %d %s \n" , errno , strerror(errno));
				break;
			}
		}
		if (!strncmp(wbuf, "exit", 4)) {
			printf("This process is about to exit....\n");
			break;
		}
	}


	close(client);
	return 0;
}
