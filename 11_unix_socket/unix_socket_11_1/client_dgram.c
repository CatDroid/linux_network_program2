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

#define SERVER_UNIX_PATH "/tmp/unix_socket_dgram"
#define REFUSED "refuse\n"
#define USING_ABSTRACT_ADDRESS 1

int main(int argc, char**argv) {
	char rbuf[256];
	char wbuf[255];
	int sock;
	struct sockaddr_un localaddr;
	struct sockaddr_un targetaddr;
	int reSend;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("socket error");
		exit(1);
	} else {
		printf("socket success\n");
	}

	bzero(&localaddr, sizeof(localaddr));
	localaddr.sun_family = AF_LOCAL;


	const char* temp_path = tmpnam(NULL);// /tmp/fileqOTqMB
	printf("temp unix socket addr %s\n" , temp_path );
#if  USING_ABSTRACT_ADDRESS
	strncpy(localaddr.sun_path + 1 , temp_path , strlen(temp_path));
	localaddr.sun_path[0] = '\0' ;
	// 抽象本地地址 1. 需要用 sizeof 精确地址长度 !
	//			  2. 不需要unlink
	if (bind(sock, (struct sockaddr*)&localaddr, sizeof(localaddr.sun_family) + 1 + strlen(temp_path) ) == -1){
		printf("bind error %d %s \n",errno, strerror(errno));
		return -1;
	}
#else
	strncpy(localaddr.sun_path , temp_path , strlen(temp_path));
	if (bind(sock, (struct sockaddr*)&localaddr, SUN_LEN(&localaddr)) == -1){
		printf("bind error %d %s \n",errno, strerror(errno));
		return -1;
	}
#endif

	// connect之后就不用sendto/recefrom 直接write和read
	// 不管是 字节流 还是 数据报 套接字 connect都会检查 socket是否存在
	// 如果这里不connect的话 sendto时候也会检查
	bzero(&targetaddr, sizeof(struct sockaddr_un));
	targetaddr.sun_family = AF_UNIX;
	strncpy(targetaddr.sun_path , SERVER_UNIX_PATH , strlen(SERVER_UNIX_PATH)); // 不需要包含\0 根据connect时候设置的长度
	if (!connect(sock, (struct sockaddr *) &targetaddr,
			SUN_LEN(&targetaddr ))) { // SUN_LEN 是用 strlen(sockaddr_un.sun_path)的 不包含'\0'
		printf("connect ok\n");
	} else {
		printf("connect errno %d %s\n" , errno ,strerror(errno));
		sleep(20);
		close(sock);
		return -1;
	}


	signal(SIGPIPE,SIG_IGN); // 这样防止了SIGPIPE 默认处理程序 杀掉进程  write返回EPIPE

	while (1) {

		bzero(wbuf, sizeof(wbuf));
		printf("please enter information:\n");
		fgets(wbuf, sizeof(wbuf), stdin);
		reSend = 0;
		reSendData: if (write(sock, wbuf, strlen(wbuf)) <= 0) {
			perror("write error");
			break;
		} else {
			printf("wait for server ack\n");
			bzero(rbuf, sizeof(rbuf));
			if (read(sock, rbuf, sizeof(rbuf)) > 0) {
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
#if  USING_ABSTRACT_ADDRESS
	printf("使用抽象地址空间 close时候会删除socket\n");
#else
	printf("使用基于文件系统的绝对地址 需要手动unlink文件\n");
	unlink(temp_path); //  srwxrwxr-x 1 hanlon hanlon 0 10月 30 16:48 /tmp/fileqOTqMB
#endif

	close(sock);
	return 0;
}
