#include <sys/types.h>
#include <sys/socket.h>	// AF_UNIX
#include <sys/un.h>		// struct sockaddr_un
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>		// fcntl F_SETFL O_NONBLOCK
#include <sys/select.h>	// FD_SET FD_ZERO fd_set

#define UNIX_PATH "/tmp/unix_socket"
#define REFUSED "refuse\n"

int main(int argc, char**argv) {
	char rbuf[256];
	char wbuf[255];
	int sock;
	struct sockaddr_un targetaddr;
	int reSend;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket error");
		exit(1);
	} else {
		printf("socket success\n");
	}
	bzero(&targetaddr, sizeof(struct sockaddr_un));
	targetaddr.sun_family = AF_UNIX;
	strncpy(targetaddr.sun_path , UNIX_PATH , strlen(UNIX_PATH)); // 不需要包含\0 根据connect时候设置的长度
	if (!connect(sock, (struct sockaddr *) &targetaddr,
			SUN_LEN(&targetaddr ))) { // SUN_LEN 是用 strlen(sockaddr_un.sun_path)的 不包含'\0'
		printf("connect ok\n");

		bzero(rbuf, sizeof(rbuf));
		if (read(sock, rbuf, sizeof(rbuf)) > 0) {
			printf("%s\n", rbuf);
			if( !strncmp(rbuf , REFUSED ,strlen(REFUSED)) ){
				close(sock);
				return 0;
			}
		}

	} else {
		printf(">>>>>>>>>>>>>>>>>>>>>> connect errno %d %s\n" , errno ,strerror(errno));
		//unix socket
		//		1.path不存在的情况 返回 errno ENOENT 2  No such file or directory
		//		2.ath不是socket节点 返回 errno ECONNREFUSED  111 Connection refused
		//		3.path存在 并是socket节点 但是没有打开(服务端bind) 那么也是返回 111 Connection refused
		//		4.类型不符号   返回 errno 91 Protocol wrong type for socket
		//		5.没有权限(跟open的只写检查权限一样) errno 13 Permission denied
		exit(1);
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
		reSendData: if (write(sock, wbuf, strlen(wbuf)) <= 0) {
			perror("write error"); // 服务端拒绝了 就会close掉socket 如果还写入 会导致SIGPIPE信号 杀掉进程
			break;
		} else {
			printf("wait for server ack\n");
			bzero(rbuf, sizeof(rbuf));
			if (read(sock, rbuf, sizeof(rbuf)) > 0) {
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
			} else {
				printf("read error %d %s \n" , errno , strerror(errno));
				break;
			}

		}

		if (!strncmp(wbuf, "exit", 4)) {
			printf("This process is about to exit....\n");
			break;
		}
	}
	close(sock);
	return 0;
}
