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

#define WELCOME "welcome to server 2016\n"
#define REFUSED "refuse\n" // too many clients to server 2016
#define UNIX_PATH "/tmp/unix_socket"
static void sigint_handler(int signo) {
	printf("catch signal!\n");
	return;
}

static void socket_signal_io_handler(int signo){
	printf("catch IO signal!\n");
	return;
}

static void socket_signal_pipe_handler(int signo){
	printf("catch pipe signal!\n");
	return;
}


int main(int argc, char*argv[]) {
	int error = 0; /*错误值*/
	int sock_UNIX; /*socket*/
	struct sockaddr_un addr_UNIX; /*AF_UNIX族地址*/
	int len_UNIX; /*AF_UNIX族地址长度*/

	sock_UNIX = socket(AF_UNIX, SOCK_STREAM, 0); // ??? SOCK_DRAGM ??? 会是怎么样???
	if (sock_UNIX == -1) {
		perror("create socket error");
		return -1;
	}

	//由于之前已经使用了path路径用于其他用途,需要将之前的绑定取消
	unlink(UNIX_PATH);

	//填充地址结构
	memset(&addr_UNIX, 0, sizeof(addr_UNIX));
	addr_UNIX.sun_family = AF_LOCAL; // = AF_UNIX
	strncpy(addr_UNIX.sun_path,UNIX_PATH, strlen(UNIX_PATH)); // 需要拷贝 是个字符数组
	len_UNIX = sizeof(struct sockaddr_un);

	// UNIX_PATH_MAX <= sys/un.h 可能没有这个定义了
	printf("sizeof = %d  SUN_LEN = %d  \n", len_UNIX, SUN_LEN(&addr_UNIX));
	printf("sa_family_t = %zd\n",
			(size_t) (((struct sockaddr_un*) 0)->sun_path));
	// # define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)  \
    // 							+ strlen ((ptr)->sun_path))
	// sizeof = 110  SUN_LEN = 18
	// sa_family_t = 2

	// 绑定地址到socket sock_UNIX
	error = bind(sock_UNIX, (struct sockaddr*) &addr_UNIX, len_UNIX); // SUN_LEN(&addr_UNIX)
	if (error == -1) {
		perror("bind error");
		return -1;
	} // srwxrwxr-x 1 hanlon hanlon 0 10月 29 23:15 /tmp/unix_socket

	fcntl(sock_UNIX, F_SETFL, O_NONBLOCK);

	signal(SIGINT, sigint_handler); // 设置SIGINT 可以使 select 返回 -1 和 error = EINTR
	//signal(SIGINT,SIG_IGN);		// 设置SIGINT忽略 会导致select不能返回-1

	//signal(SIGIO,socket_signal_io_handler);

	//signal(SIGPIPE,socket_signal_pipe_handler);
	signal(SIGPIPE,SIG_IGN);		// 设置 SIGPIPE 为处理或者SIG_IGN
									// 可以 在write对端已经closed的socket时候返回 EPIPE 32 Broken pipe
									// 往一个已经关闭的socket写入数据会触犯SIGPIPE

#define MAX_CLIENT 1
	listen(sock_UNIX, MAX_CLIENT ); // 测试队列满: listen将一个socket由默认的主动连接套接字 变成 被动连接套接字
									// 			 这个不能控制连接的客户端数目 只能控制同一时间 TCP并发连接请求数目

	int isconnect[MAX_CLIENT];
	bzero(isconnect, MAX_CLIENT * sizeof(int)); // 必须要做 否者引发 select errno=9 EBADF  Bad file number
	int maxfd = sock_UNIX;
	int client_start = sock_UNIX + 1;
	int exitflag = 0;
	// 用于read write
	char rbuf[256];
	char wbuf[256];
	while (!exitflag) {
		fd_set fds;
		int fd_index = 0;
		int fd_client = 0 ;
		int change = 0;
		FD_ZERO(&fds);
		FD_SET(sock_UNIX, &fds);
		for (fd_index = 0 ; fd_index < MAX_CLIENT; fd_index++) {
			if (isconnect[fd_index]) {
				fd_client = client_start + fd_index ;
				printf("set fd %d\n", fd_client );
				if (fd_client > maxfd)
					maxfd = fd_client;
				FD_SET( fd_client, &fds);
			}
		}
		if (change = select(maxfd + 1, &fds, NULL, NULL, NULL), change > 0) {
			//printf("change=%d\n",change);
			int fd = 0 ;
			for ( fd = sock_UNIX; fd <= maxfd && change > 0; fd++) {
				if (FD_ISSET(fd, &fds)) {
					change--;
					if (sock_UNIX == fd) {	// 用于监听的socket

						struct sockaddr_un client_addr;
						int len = sizeof(struct sockaddr_un);

						bzero(&client_addr, sizeof(struct sockaddr_un));
						int sock_client = accept(sock_UNIX,
								(struct sockaddr*) &client_addr, &len);
						if (sock_client == -1) {
							perror("accept error");
							continue;
						}

						// 处理协议栈发出的SIGIO信号
						//fcntl(sock_client,F_SETOWN, getpid()); // 	设置sock的拥有者为本进程
						//int flags = fcntl(sock_client,F_GETFL);//	设置sock支持异步通知
						//fcntl(sock_client,F_SETFL, flags|O_ASYNC);
						//printf("set stack SIGIO signal hanlde async\n");


						printf("connect from %d len:%d path:%s[%c %c %c %c ]\n",
								sock_client, len , client_addr.sun_path,
								client_addr.sun_path[1],
								client_addr.sun_path[2],
								client_addr.sun_path[3],
								client_addr.sun_path[4]);
						// 字节流套接字  	a.如果客户不bind地址/路径名字 accept不会返回客户端的地址/路径信息信息
						//				b.由于SOCK_STREAM基于连接的,可以通过accpet返回的socket来发送数据给客户端
						// 数据包套接字	a.如果客户不bind地址/路径名字 recefrom就不能获得客户端的地址/路径信息
						//				b.服务端没有recefrom返回的路径信息 所以不能通过sendto返回数据给客户端

						fd_index = sock_client - client_start ;
						if( fd_index  >= MAX_CLIENT){ // 客户端数目过多
							write(sock_client, REFUSED, sizeof(REFUSED));
							close(sock_client);
							printf("too many clients , refuse sock_client %d  index %d \n",sock_client , fd_index );
						}else{
							isconnect[sock_client - client_start ] = 1;
							FD_SET(sock_client, &fds);
							write(sock_client, WELCOME, sizeof(WELCOME));
							printf("new client sock_client %d  index %d \n",sock_client , fd_index );
						}

					} else {			// 其他已经建立连接的socket

						int read_size = 0;
						bzero(rbuf, sizeof(rbuf));
						errno = 0 ;
						if (read_size = read(fd, rbuf, sizeof(rbuf) - 1), read_size
								> 0) { // 对方只能发255个数据
							rbuf[read_size - 1] = '\0';
							printf("recv from %d : %d , %s\n", fd, read_size,
									rbuf);
							//fflush(stdout); //应对 输出缓冲区不满/没有回车符\n
							if (write(fd, "ok", sizeof("ok")) <= 0) {
								printf("write error %d %s\n", errno , strerror(errno));
								if(errno == EPIPE){ 	// 有概率可能 这时候客户端就调用了close
														// 处理情况 会抛出信号 SIGPIPE 需要使用signal(SIGPIPE,处理函数/SIG_IGN)
														// 这样才能是进程不会被杀掉(默认SIGPIPE处理是杀掉进程)
									printf("client may be closed , please reconnect\n");
									isconnect[fd - client_start] = 0;
									FD_CLR(fd, &fds);
									close(fd);
									printf("close %d\n", fd);
								}else{
									printf("unknown error ! no handle yet ! \n");
								}
							}else{
								printf("reply ok to %d done\n" , fd);
								if (!strncmp(rbuf, "exit", 4)) {
									isconnect[fd - client_start] = 0;
									FD_CLR(fd, &fds);
									close(fd);
									printf("close %d\n", fd);
									break;
								}
							}

						}
						else if(read_size == 0 ){ // 客户端关闭 首先read会返回 0 没有错误码
							printf("peer close %d %s \n", errno ,strerror(errno));
							isconnect[fd - client_start] = 0;
							FD_CLR(fd, &fds);
							close(fd);
							continue;
						}
						else {
							if (errno == ECONNRESET) { // 客户端发送数据后立刻主动调用了close 然后客户端继续read就会导致 /或者客户端发送TCP包带RST位
								perror("read error");
								printf( "client %d connect reset \n", fd);
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &fds);
								close(fd);
								continue;
							} else {
								printf("unknown read error (%d %s) , try write \n", errno, strerror(errno));
							}
							int write_size = 0;
							if (write_size = write(fd, "error", sizeof("error")), write_size <= 0) {
								printf("read error and then write error too fd %d errno %d %s\n" , fd , errno ,strerror(errno) );
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &fds);
								close(fd);
							} else {
								printf("read error but write success ,and  write_size = %d\n", write_size);
							}
							// 1.write时候 另外一段close  --> SIGPIPE 信号
							// 2.select另一端直接被杀掉		--> select返回 read返回0 EOF
							// read error 104 Connection reset by peer 这时候如果在write的话 会导致进程收到??信号而退出
						}

					}

				}
			}

		} else if (change == -1) {
			printf("select error:%d,%s\n", errno, strerror(errno));
			// error:4,Interrupted system call
			// 如果按下Ctrl+C select会返回EINTR Interrupted system call
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
