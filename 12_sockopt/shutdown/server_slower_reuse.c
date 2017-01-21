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
#include <netinet/tcp.h> // TCP_INFO

#define WELCOME "welcome to server 2016\n"
#define REFUSED "refuse\n"
#define LOCAL_PORT 8899
#define BACKLOG 5

static void signal_handler(int signo) {
	switch (signo) {
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

unsigned int dumpTCPstate(int socket , const char* stage )
{
	struct tcp_info optval;
	int tcp_info_len = sizeof(struct tcp_info);
	int ret=  getsockopt(socket, IPPROTO_TCP, TCP_INFO, &optval, &tcp_info_len);
	printf("[%s]connect status is %d \n" , (stage!=NULL)?stage:"unknown" , optval.tcpi_state);
	return optval.tcpi_state;
}

int main(int argc, char**argv) {

	int ret = 0;
	int server = -1;
	int pid = getpid();

	server = socket(AF_INET, SOCK_STREAM, 0);

	int optval = 1;
	ret = setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
			sizeof(optval));
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

	socklen_t rcv_size = 4096;
	int optlen = sizeof(rcv_size);
	ret = setsockopt(server, SOL_SOCKET, SO_RCVBUF, &rcv_size, optlen);
	if(ret < 0 ){ 			// 会传递给client的socket 不能少于 4608
		printf("设置发送缓冲区大小错误 %d %s \n",errno ,strerror(errno));
	}

	ret = listen(server, BACKLOG);
	if (ret < 0) {
		printf("listen error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}

	signal(SIGINT, signal_handler);

	int isconnect[BACKLOG];
	bzero(isconnect, BACKLOG * sizeof(int));
	int maxfd = server;
	int client_start = server + 1;
	int exitflag = 0;
	// 用于read write
	char rbuf[256];
	char wbuf[256];
	while (!exitflag) {

		fd_set readfds;
		FD_ZERO(&readfds);

		int fd_index = 0;
		int fd_client = 0;
		int change = 0;

		FD_SET(server, &readfds);
		for (fd_index = 0; fd_index < BACKLOG; fd_index++) {
			if (isconnect[fd_index]) {
				fd_client = client_start + fd_index;
				FD_SET(fd_client, &readfds);
				if (fd_client > maxfd)
					maxfd = fd_client;
			}
		}
		if (change = select(maxfd + 1, &readfds, NULL, NULL, NULL), change > 0) {
			int fd = 0;
			for (fd = server; fd <= maxfd && change > 0; fd++) {

				if (FD_ISSET(fd, &readfds)) {
					change--;
					if (server == fd) {
						struct sockaddr_in client_addr;
						int len = sizeof(struct sockaddr_in);
						bzero(&client_addr, sizeof(struct sockaddr_in));
						int sock_client = accept(server,
								(struct sockaddr*) &client_addr, &len);
						if (sock_client == -1) {
							printf("accept error %d %s\n", errno,
									strerror(errno));
							continue;
						}

						printf("connect from %d len %d ip %s  port %d \n",
								sock_client, len,
								inet_ntoa(client_addr.sin_addr),
								ntohs(client_addr.sin_port));

						fd_index = sock_client - client_start;
						if (fd_index >= BACKLOG) { // 客户端数目过多
							write(sock_client, REFUSED, sizeof(REFUSED));
							close(sock_client);
							printf(
									"too many clients , refuse sock_client %d  index %d \n",
									sock_client, fd_index);
						} else {
							isconnect[sock_client - client_start] = 1;
							FD_SET(sock_client, &readfds);
							write(sock_client, WELCOME, sizeof(WELCOME));
							printf("new client sock_client %d  index %d \n",
									sock_client, fd_index);
						}

					} else {			// 其他已经建立连接的socket

						int read_size = 0;
						bzero(rbuf, sizeof(rbuf));
						errno = 0;
						if (read_size = read(fd, rbuf, sizeof(rbuf) - 1),
								read_size > 0) {
							rbuf[read_size] = '\0';
							printf("recv from %d : %d , %s\n", fd, read_size,rbuf);

							// make server slower
							sleep(2);

							// 测试如果对方发送了FIN (FIN_WAIT1)
							// 但是本段还有数据没有接收完毕 还没有发送FIN_ACK(依旧ESTABLISHED)
							// 这时候 对方还是可以接收到 select还是可以接收到数据的!
//							static int counter = 7;
//							if( --counter == 0 ){
//								errno = 0 ;
//								ret = write(fd, "1" , 1 );
//								printf("write  ret = %d  errno = %d %s\n", ret , errno ,strerror(errno) );
//							}

						} else if (read_size == 0) { // 客户端关闭 首先read会返回 0 没有错误码
							printf("peer close %d %s \n", errno,
									strerror(errno));
							dumpTCPstate( fd , "readsize==0");
							isconnect[fd - client_start] = 0;
							FD_CLR(fd, &readfds);
							close(fd);
							continue;
						} else {
							if (errno == ECONNRESET) { // 客户端发送数据后立刻主动调用了close 然后客户端继续read就会导致 /或者客户端发送TCP包带RST位
								printf("client %d connect reset %d %s \n", fd,
										errno, strerror(errno));
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &readfds);
								close(fd);
								continue;
							} else {
								printf("unknown read error (%d %s) , try write \n",
										errno, strerror(errno));
							}
							int write_size = 0;
							if (write_size = write(fd, "error",
									sizeof("error")), write_size <= 0) {
								printf(
										"read error and then write error too fd %d errno %d %s\n",
										fd, errno, strerror(errno));
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &readfds);
								close(fd);
							} else {
								printf(
										"read error but write success ,and  write_size = %d\n",
										write_size);
							}
						}
					}

				}
			}

		} else if (change == -1) {
			printf("select error:%d,%s\n", errno, strerror(errno));
			if (errno == EINTR) {
				exitflag = 1;
				for (fd_index = 0; fd_index < BACKLOG; fd_index++) {
					if (isconnect[fd_index]) {
						close(fd_index + client_start);
						isconnect[fd_index] = 0;
					}
				}
				printf("close all clients\n");
			}
		} else if (change == 0) {
			printf("timeout\n");
		}
	}

	close(server);

	return 0;

}
