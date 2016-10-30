#include <sys/types.h>
#include <sys/socket.h>	// AF_UNIX
#include <sys/un.h>		// struct sockaddr_un
#include <string.h>
#include <signal.h>		// signum.h SIGPIPE SIGIO SIGINT SIGALRM  SIG_IGN
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>		// fcntl F_SETFL O_NONBLOCK
#include <sys/select.h>	// FD_SET FD_ZERO fd_set

#define WELCOME "welcome to dgram server 2016\n"
#define REFUSED "refuse\n" // too many clients
#define UNIX_PATH "/tmp/unix_socket_dgram"
static void sigint_handler(int signo) {
	printf("catch signal!\n");
	return;
}

int main(int argc, char*argv[]) {
	int error = 0; /*错误值*/
	int sock_UNIX; /*socket*/
	struct sockaddr_un addr_UNIX; /*AF_UNIX族地址*/

	// 1. 提供边界
	// 2. 既不丢失 也不乱序   2.6 提供 SOCK_SEQPACKET
	// 3. sendto也会阻塞 但是UDP下的sendto如果网络快的话一般立刻放回 除非发送频率高 网络差 导致socket缓冲区满 这才会阻塞
	sock_UNIX = socket(AF_UNIX, SOCK_DGRAM, 0); // 数据报套接字
	if (sock_UNIX == -1) {
		perror("create socket error");
		return -1;
	}

	unlink(UNIX_PATH);

	memset(&addr_UNIX, 0, sizeof(addr_UNIX));
	addr_UNIX.sun_family = AF_UNIX;
	strncpy(addr_UNIX.sun_path,UNIX_PATH, strlen(UNIX_PATH));


	error = bind(sock_UNIX, (struct sockaddr*) &addr_UNIX, SUN_LEN(&addr_UNIX));
	if (error == -1) {
		perror("bind error");
		return -1;
	}

	fcntl(sock_UNIX, F_SETFL, O_NONBLOCK);

	signal(SIGINT, sigint_handler);
	signal(SIGPIPE,SIG_IGN);


	int exitflag = 0;
	// 用于read write
	char rbuf[16]; // 测试数据报被截断
	char wbuf[256];
	while (!exitflag) {
		fd_set fds;
		int fd_index = 0;
		int fd_client = 0 ;
		int change = 0;
		FD_ZERO(&fds);
		FD_SET(sock_UNIX, &fds);
		if (change = select(sock_UNIX + 1, &fds, NULL, NULL, NULL), change > 0) {

			struct sockaddr_un from_addr;
			memset(&from_addr, 0 ,sizeof(struct sockaddr_un));
			int from_len = sizeof(struct sockaddr_un);
			int count = recvfrom( sock_UNIX, rbuf, sizeof(rbuf)-1, MSG_TRUNC,
						( struct sockaddr*) &from_addr, &from_len );
			// 不会填充地址信息 from !返回 from_len = 0
			// 如果客户端不bind定自己的本地地址/路径

			if( count > sizeof(rbuf) - 1){
				printf("消息过大 已被截断\n");
				rbuf[sizeof(rbuf)-1] = '\0';
			}else if( count > 0 ) {
				rbuf[count] = '\0' ;
			}else if( count <= 0 ){
				printf("recvfrom error %d %s\n", errno , strerror(errno));
				continue ;
			}
			printf("count = %d %s\n" , count , rbuf );

			printf("reply to client [%d %d --%c%c%c%c%c%c%c%c%c%c--]done!\n",
													from_len,
													from_addr.sun_family ,
													from_addr.sun_path[0],
													from_addr.sun_path[1],
													from_addr.sun_path[2],
													from_addr.sun_path[3],
													from_addr.sun_path[4],
													from_addr.sun_path[5],
													from_addr.sun_path[6],
													from_addr.sun_path[7],
													from_addr.sun_path[8],
													from_addr.sun_path[9]
													);



			// 如果客户端/发送者使用匿名的路径,而用recvfrom返回的地址sendto 会得到错误号 22 Invalid argument
			// 客户端/发送者使用匿名(不bind路径),不能回复
			if( from_len != 0){
				int sendsize  = -1 ;
				if(from_addr.sun_path[0] == '\0'){
					printf("发送端/客户端使用抽象地址空间 abstract namespace 地址长度要用 recefrom的返回值 %d 或者精确计算 %lu \n",
							 from_len,
							 sizeof(from_addr.sun_family) + 1 + strlen(from_addr.sun_path+1)  );
					sendsize = sendto(sock_UNIX, "ok", sizeof("ok"),
									0, (struct sockaddr*)&from_addr,  from_len );
				}else{
					printf("发送端/客户端使用基于文件系统的绝对路径 地址长度可以sizeof sockaddr_un = 110 \n");
					sendsize = sendto(sock_UNIX, "ok", sizeof("ok"),
									0, (struct sockaddr*)&from_addr, sizeof(struct sockaddr_un));
				}

				if(sendsize <= 0 ){
					printf("sendto error ! sendsize = %d errno = %d %s\n" ,sendsize ,errno , strerror(errno));
				}
			}else if(from_len == 0){
				printf("发送端/客户端使用匿名路径 不能回复/sendto\n");
			}
		} else if (change == -1) {
			printf("select error:%d,%s\n", errno, strerror(errno));
			if (errno == EINTR) {
				exitflag = 1;
			}
		} else if (change == 0) {
			printf("timeout\n");
		}

	}

	close(sock_UNIX);
	unlink(UNIX_PATH);

	return 0;
}
