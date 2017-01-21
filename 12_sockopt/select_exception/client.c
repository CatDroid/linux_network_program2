
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

// wireshark 写法
// tcp.dstport == 8899 || tcp.srcport == 8899

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

		if (write(client, wbuf, strlen(wbuf)) <= 0) {
			printf("write error %d %s\n" ,errno ,strerror(errno));
			break;
		} else {
			// 测试Excpetion
			break;
			bzero(rbuf, sizeof(rbuf));
			if (ret = read(client, rbuf, sizeof(rbuf) - 1 ), ret  > 0) {
				rbuf[ret] = '\0' ;
				printf("server recvice data ack %s\n", rbuf);
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
