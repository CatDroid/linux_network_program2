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
#define LOCAL_PORT 8899

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

/*
 * 一个TCP/UDP连接是被一个五元组确定的
 * 任何两个连接都不可能拥有相同的五元组，否则系统将无法区别这两个连接。
  	当使用socket()函数创建套接字的时候，我们就指定了该套接字使用的protocol(协议)
  	bind()函数设置了源地址和源端口号
	connect()函数设定目的地址和目的端口号

	任意端口:自动分配具体端口
	如果你明确绑定一个socket，把它绑定到端口0是可行的，它意味着"any port"("任意端口")
	由于一个套接字无法真正的被绑定到系统上的所有端口
	那么在这种情况下系统将不得不选择一个具体的端口号（指的是"any port"）

	通配地址:
	源地址使用类似的通配符
	也就是"any address" （IPv4中的0.0.0.0和IPv6中的::）。
	和端口不同的是，一个套接字可以被绑定到任意地址(any address)，这里指的是本地网络接口的所有地址。

	由于socket无法在连接的时候同时绑定到所有源IP地址，因此当接下来有一个连接过来的时候，系统将不得不挑选一个源IP地址。
	考虑到目的地址和路由表中的路由信息，系统将会选择一个合适的源地址
	并将任意地址替换为一个选定的地址作为源地址。(connect时候 会把通配地址 换成 具体地址 )


	通配地址 bind冲突:
	一个socket可能绑定到本地"any address"。例如一个socket绑定为 0.0.0.0:21，
	那么它同时绑定了所有的本地地址
	在这种情况下，不论其它的socket选择什么特定的IP地址，它们都无法绑定到21端口
	因为0.0.0.0和所有的本地地址都会冲突
	(除非使用 SO_REUSEPORT)

	地址重用:
	当涉及到地址重用的时候，OS之间的差异就显现出来了
	blog.chinaunix.net/uid-28587158-id-4006500.html
	SO_REUSEADDR       socketA        socketB       Result
	---------------------------------------------------------------------
	  ON/OFF       192.168.0.1:21   192.168.0.1:21    Error (EADDRINUSE)
	  ON/OFF       192.168.0.1:21      10.0.0.1:21    OK
	  ON/OFF          10.0.0.1:21   192.168.0.1:21    OK
	   OFF             0.0.0.0:21   192.168.1.0:21    Error (EADDRINUSE)
	   OFF         192.168.1.0:21       0.0.0.0:21    Error (EADDRINUSE)
	   ON              0.0.0.0:21   192.168.1.0:21    OK	(在linux上监听(listening)TCP不可以,只要有一个已在linten的socket 其他socket就不能再bind同一个端口,非监听的TCP可以)
	   ON          192.168.1.0:21       0.0.0.0:21    OK 	(同上Linux)
	  ON/OFF           0.0.0.0:21       0.0.0.0:21    Error (EADDRINUSE)



  	  connect返回
  	  一个连接是被一个五元组定义的。
  	  同样我也说了任意两个连接的五元组不能完全一样，因为这样的话内核就没办法区分这两个连接了。
  	  然而，在地址重用的情况下，
  	  你可以把同协议的两个socket绑定到完全相同的源地址和源端口，
  	  这意味着五元组中已经有三个元素相同了(协议，源地址，源端口)。

  	  如果你尝试将这些socket连接到同样的目的地址和目的端口，你就创建了两个完全相同的连接。
  	  这是不行的，至少对TCP不行(UDP实际上没有真实的连接)。
  	  如果数据到达这两个连接中的任何一个，那么系统将无法区分数据到底属于谁。
  	  因此当源地址和源端口相同时，目的地址或者目的端口必须不同，否则内核无法进行区分，
  	  这种情况下，connect()将在第二个socket尝试连接时返回EADDRINUSE。
 */

int main(int argc, char**argv) {

	int ret = 0;
	int server = -1;
	int pid = getpid();

	server = socket(AF_INET, SOCK_STREAM, 0);

	/*
	 * 	关于TIME_WAIT和LISTEN的端口：
	 *
	 *  如果关闭服务前有客户端连接过, 即使关闭了客户端套接字 还会有短暂 TCP保持端口连接 TIME_WAIT
	 	tcp        0      0 127.0.0.1:8899          127.0.0.1:46130         TIME_WAIT
		这样再启动同一个程序 bind就会出错

		如果关闭服务前没有客户端连接, 也就是只有服务端的socket在listen如下
		tcp        0      0 0.0.0.0:8899            0.0.0.0:*               LISTEN
		close服务端的socket之后 就会关闭这个listen状态的端口


		在linux 3.9之前，只存在选项SO_REUSEADDR
		除了两个重要的差别，大体上与BSD一样。

		第一个差别：当一个监听(listening)TCP socket绑定到通配地址和一个特定的端口，
				无论其它的socket或者是所有的socket(包括监听socket)都设置了SO_REUSEADDR，
				其它的TCP socket都无法绑定到相同的端口(BSD中可以)，就更不用说使用一个特定地址了。
				这个限制并不用在非监听TCP socket上，当一个监听socket绑定到一个特定的地址和端口组合，
				然后另一个socket绑定到通配地址和相同的端口，这样是可行的。

		第二个差别: 当把SO_REUSEADDR用在UDP socket上时，它的行为与BSD上SO_REUSEPORT完全相同，
				因此两个UDP socket只要都设置了SO_REUSEADDR，那么它们可以绑定到相同的地址和端口。
				都可以接受一样的广播报文

    	Linux 3.9加入了SO_REUSEPORT。这个选项允许多个socket(TCP or UDP)
    			不管是监听socket还是非监听socket只要都在绑定之前都设置了它，
    			那么就可以绑定到完全相同的地址和端口。
    			为了阻止"port 劫持"(Port hijacking) 有一个特别的限制：
    			所有希望共享源地址和端口的socket都必须拥有相同的有效用户id(effective user ID)。
    			因此一个用户就不能从另一个用户那里"偷取"端口。

    			另外，内核在处理SO_REUSEPORT socket的时候使用了其它系统上没有用到的"特别魔法"：
    			对于UDP socket，内核尝试平均的转发数据报,但是可以接受同样的广播包
    			对于TCP监听socket，内核尝试将新的客户连接请求(由accept返回)平均的交给共享同一地址和端口的socket(监听socket)

    			这意味着在其他系统上socket收到一个数据报或连接请求或多或少是随机的，
    			但是linux尝试优化分配。
    			例如：一个简单的服务器程序的多个实例可以使用SO_REUSEPORT socket
    				实现一个简单的负载均衡，因为内核已经把复制的分配都做了
	 */

	int optval = 1 ;
	ret = setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const void *)&optval,sizeof(optval));
	if (ret < 0) {
		printf("setsockopt SO_REUSEADDR %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}

	/* 可以单独使用SO_REUSEPORT
	 * tcp        0      0 0.0.0.0:8899            0.0.0.0:*               LISTEN
	 * tcp        0      0 0.0.0.0:8899            0.0.0.0:*               LISTEN
	 * 这样两个服务进程会 负载均衡
	 * TCP:平均分配客户请求accept
	 * UDP:平均分配客户端发送的数据
	 * */
//	int optval_reuse_port = 1 ;
//	ret = setsockopt(server,SOL_SOCKET,SO_REUSEPORT,(const void *)&optval_reuse_port,sizeof(optval_reuse_port));
//	if (ret < 0) {
//		printf("setsockopt SO_REUSEADDR %d %s\n", errno, strerror(errno));
//		close(server);
//		return -1;
//	}

	/* SO_REUSEADDR 针对端口忙 而且处于 TIME_WAIT的端口 可重新绑定
	 *
	 * SO_REUSEPORT  可以同一个EUID的多个进程 绑定 同一个端口和IP
	 *
	 *	As this socket option appears with kernel 3.9 and raspberry use 3.12.x,
	 *	it will be needed to set SO_REUSEPORT.
	 *
       SO_REUSEPORT (since Linux 3.9)
              Permits multiple AF_INET or AF_INET6 sockets to be bound to an 允许AF_INET的套接字绑定到同一个地址
              identical socket address.  This option must be set on each	 必须被所有调用bind的socket调用 包含第一个
              socket (including the first socket) prior to calling bind(2)
              on the socket.  To prevent port hijacking  , all of the		为防止绑架端口 所有绑定到一个端口的进程(两个同时运行,另外一个进程在源进程异常后接管了端口)
              processes binding to the same address must have the same 		必须有相同的UID
              effective UID.  This option can be employed with both TCP and TCP/UDP都可以使用
              UDP sockets.

		1. 即使后面运行的是root用户  也不能reuse其他用户的端口
		2. 两个进程同时reuse一个端口 只有第一个bind的进程能够收到数据 (负载均衡)

		3. 在调用bind之前设置SO_REUSEADDR套接口选项 否则其他程序不能reuse
		4. SO_REUSEADDR用于对TCP套接字处于TIME_WAIT状态下的socket，才可以重复绑定使用

		5. TCP，先调用close()的一方会进入TIME_WAIT状态

			内核在处理SO_REUSEPORT socket的时候使用了其它系统上没有用到的"特别魔法"：
			对于UDP socket，内核尝试平均的转发数据报，
			对于TCP监听socket，内核尝试将新的客户连接请求(由accept返回)平均的交给共享同一地址和端口的socket(监听socket)

			这意味着在其他系统上socket收到一个数据报或连接请求或多或少是随机的，但是linux尝试优化分配。
			例如：一个简单的服务器程序的多个实例可以使用SO_REUSEPORT socket

			!!! 实现一个简单的负载均衡，因为内核已经把复制的分配都做了

	 *
	 * */


	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = ntohs(LOCAL_PORT);
	//local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_addr.s_addr = inet_addr("127.0.0.1");


	ret = bind(server, (struct sockaddr*) &local, sizeof(local));
	if (ret < 0) {
		printf("bind error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}



#define BACKLOG 5
	/*
	 * 0.0.0.0		通配地址
	 * 		意味着"任意本地IP地址”，也就是"所有本地IP地址“
	 * 		因此包括192.168.0.1在内的所有IP地址都被认为是已经使用了
	 *
	 * 192.168.0.1 	特定地址
	 */
	ret = listen(server, BACKLOG);
	if (ret < 0) {
		printf("listen error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}

	signal(SIGINT, signal_handler);

	// 处理协议栈发出的SIGIO信号
	fcntl(server,F_SETOWN, getpid()); 		// 	设置sock的拥有者为本进程
	int flags = fcntl(server,F_GETFL);		//	设置sock支持异步通知
	fcntl(server,F_SETFL, flags|O_ASYNC);
	signal(SIGIO,signal_handler);

	int isconnect[BACKLOG];
	bzero(isconnect, BACKLOG * sizeof(int)); // 必须要做 否者引发 select errno=9 EBADF  Bad file number
	int maxfd = server;
	int client_start = server + 1;
	int exitflag = 0;
	// 用于read write
	char rbuf[256];
	char wbuf[256];
	while (!exitflag) {

		fd_set readfds; FD_ZERO(&readfds);

		int fd_index = 0;
		int fd_client = 0 ;
		int change = 0;

		FD_SET(server, &readfds);
		for (fd_index = 0 ; fd_index < BACKLOG; fd_index++) {
			if (isconnect[fd_index]) {
				fd_client = client_start + fd_index ;
				FD_SET(fd_client, &readfds);
				if (fd_client > maxfd)
					maxfd = fd_client;
			}
		}
		if (change = select(maxfd + 1, &readfds, NULL, NULL, NULL), change > 0) {
			int fd = 0 ;
			for ( fd = server; fd <= maxfd && change > 0; fd++) {



				if (FD_ISSET(fd, &readfds)) {
					change--;
					if (server == fd) {
						struct sockaddr_in client_addr;
						int len = sizeof(struct sockaddr_in);
						bzero(&client_addr, sizeof(struct sockaddr_in));
						int sock_client = accept(server,(struct sockaddr*) &client_addr, &len);
						if (sock_client == -1) {
							printf("accept error %d %s\n",errno,strerror(errno));
							continue;
						}


						/*
						 * server_reuse.c:118:12:
						 * warning: format ‘%s’ expects argument of type ‘char *’,
						 * 	but argument 4 has type ‘int’ [-Wformat=]
            				inet_ntoa(client_addr.sin_addr) );
						 * 必须要加inet_ntoa的头文件 ！！！
						 * #include <arpa/inet.h>
						 *
						 */
						printf("connect from %d len %d ip %s  port %d \n",
									sock_client,
									len ,
									inet_ntoa(client_addr.sin_addr),
									ntohs(client_addr.sin_port) );

						fd_index = sock_client - client_start ;
						if( fd_index  >= BACKLOG){ // 客户端数目过多
							write(sock_client, REFUSED, sizeof(REFUSED));
							close(sock_client);
							printf("too many clients , refuse sock_client %d  index %d \n",sock_client , fd_index );
						}else{
							isconnect[sock_client - client_start ] = 1;
							FD_SET(sock_client, &readfds);
							write(sock_client, WELCOME, sizeof(WELCOME));
							printf("new client sock_client %d  index %d \n",sock_client , fd_index );
						}

					} else {			// 其他已经建立连接的socket

						int read_size = 0;
						bzero(rbuf, sizeof(rbuf));
						errno = 0 ;
						// ioctl 获取buffer ??? malloc出来
						if (read_size = read(fd, rbuf, sizeof(rbuf) - 1), read_size
								> 0) {
							rbuf[read_size] = '\0';
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
									FD_CLR(fd, &readfds);
									close(fd);
									printf("close %d\n", fd);
								}else{
									printf("unknown error ! no handle yet ! \n");
								}
							}else{
								printf("reply ok to %d done\n" , fd);
								if (!strncmp(rbuf, "exit", 4)) {
									isconnect[fd - client_start] = 0;
									FD_CLR(fd, &readfds);
									close(fd);
									printf("close %d\n", fd);
									break;
								}
							}

						}
						else if(read_size == 0 ){ 		// 客户端关闭 首先read会返回 0 没有错误码
							printf("peer close %d %s \n", errno ,strerror(errno));
							isconnect[fd - client_start] = 0;
							FD_CLR(fd, &readfds);
							close(fd);
							continue;
						}
						else {
							if (errno == ECONNRESET) { 	// 客户端发送数据后立刻主动调用了close 然后客户端继续read就会导致 /或者客户端发送TCP包带RST位
								printf( "client %d connect reset %d %s \n", fd, errno, strerror(errno));
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &readfds);
								close(fd);
								continue;
							} else {
								printf("unknown read error (%d %s) , try write \n", errno, strerror(errno));
							}
							int write_size = 0;
							if (write_size = write(fd, "error", sizeof("error")), write_size <= 0) {
								printf("read error and then write error too fd %d errno %d %s\n" , fd , errno ,strerror(errno) );
								isconnect[fd - client_start] = 0;
								FD_CLR(fd, &readfds);
								close(fd);
							} else {
								printf("read error but write success ,and  write_size = %d\n", write_size);
							}
							// 1.write时候 另外一段close  --> SIGPIPE 信号
							// 2.select另一端直接被杀掉		--> select返回 read返回0 EOF
							// 3.read的过程中 对方close   --> ECONNRESET
							// 4.read受到ECONNERESET后 这时候如果再write的话 --> SIGPIPE
						}

					}

				}
			}

		} else if (change == -1) {
			printf("select error:%d,%s\n", errno, strerror(errno));
			if (errno == EINTR) {
				exitflag = 1;
				for (fd_index = 0 ; fd_index < BACKLOG; fd_index++) {
					if (isconnect[fd_index]) {
						close(fd_index + client_start);
						isconnect[fd_index] = 0 ;
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
