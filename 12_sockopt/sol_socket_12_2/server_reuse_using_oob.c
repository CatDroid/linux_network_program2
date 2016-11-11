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
	case SIGURG:
		printf("catch SIGURG\n");
		break;
	default:
		printf("catch unknown signal %d !\n", signo);
		break;
	}
	return;
}

// 配合client_reuse.c 看到提示后输入 urg
int main(int argc, char**argv) {

	int ret = 0;
	int server = -1;
	int pid = getpid();

	server = socket(AF_INET, SOCK_STREAM, 0);

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
	//local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_addr.s_addr = inet_addr("127.0.0.1");


	ret = bind(server, (struct sockaddr*) &local, sizeof(local));
	if (ret < 0) {
		printf("bind error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}

#define BACKLOG 5
	ret = listen(server, BACKLOG);
	if (ret < 0) {
		printf("listen error %d %s\n", errno, strerror(errno));
		close(server);
		return -1;
	}

	signal(SIGINT, signal_handler);

											//	处理协议栈发出的SIGIO信号
	fcntl(server,F_SETOWN, getpid()); 		//	设置sock的拥有者为本进程
	int flags = fcntl(server,F_GETFL);		//	设置sock支持异步通知
	fcntl(server,F_SETFL, flags|O_ASYNC);
	signal(SIGIO,signal_handler);
	//signal(SIGURG,signal_handler);		//	带外数据异步信号通知

	/*
	 * SO_OOBINLINE 带外数据放入正常流中接收
	 * 先判断 sockatmark 然后直接read(不用带MSG_OOB标记)
	 * 如果sockatmark返回1 那么
	 * 那么read出来的第一个字节就是 OOB的数据(TCP URG data)
	 *
	 * 这个属性会被accept返回的套接字继承
	 *
	 * 假如发送的是 send(client,"ABCDEFGHIJKLMN" ,strlen("ABCDEFGHIJKLMN") ,MSG_OOB );
	 * 带外数据正常流中接收: read无须MSG_OOB标记
	 * recv from 4 : 13 , ABCDEFGHIJKLM
	 * recv from 4 : 1 , N
	 *
	 *
	 * blog.csdn.net/ctthuangcheng/article/details/9498569
	 *
	 * 当由紧急指针指向的实际数据字节到达接收端TCP，
	 * 该数据字节既可能被"拉出带外"，也可能被"留在带内"，即在线留存。
	 *
	 * "SO_OOBINLINE"套接字选项默认情况下是禁止的，对于这样的接收端套接字，
	 * 该数据字节并不放入"套接字接收缓冲区"，而是被放入该连接的一个独立的"单字节带外缓冲区"。
	 * 这种情况下，接收进程不能指定"MSG_OOB"标志读入该数据字节
	 * 相反，接收进程通过检查该连接的"带外标记"以获悉"何时"访问到这个数据字节
	 *
	 * 接收进程从这个"单字节缓冲区"读入数据的唯一方法是
	 * 指定MSG_OOB标志调用recv，recvfrom或recvmsg
	 *
	 * 如果新的OOB字节在旧的OOB字节被读取之前就到达，旧的OOB字节会被丢弃
	 *
	 * 错误:
	 * 1.如果接收进程请求读入"带外数据"（通过指定MSG_OOB标志）
	 * 		但是对端尚未发送任何带外数据，读入操作将返回EINVAL
	 *
	 * 2.在接收进程已被告知对端发送了一个"带外字节"（通过SIGURG或select手段）的前提下
	 *		如果接收进程读入该字节，但是该"字节尚未到达"，读入操作将返回"EWOULDBLOCK"
	 *		接收进程此时能做的仅仅是从套接字接收缓冲区读入数据
	 *		(要是没有存放这些数据的空间,可能还得丢弃他们,需要"立刻腾出协议栈buffer")
	 *		以便在该缓冲区中腾出空间，继而允许对端TCP发送出那个带外字节
	 *
	 * 3.如果接收进程试图多次读入同一个带外字节，读入操作将返回EINAVL
	 *
	 * 4.如果接收进程已经开启SO_OOBINLINE套接字选项，后来试图通过指定的MSG_OOB标志读入带外数据
	 * 	 读入操作将返回EINVAL
	 */
#if 0
	int on = 1 ;
	ret = setsockopt(server, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on));
	if( ret < 0 ){
		printf("setsockopt SO_OOBINLINE error %d %s\n",errno,strerror(errno));
	}else{
		printf("带外数据在线接收 启用\n");
	}
#endif


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
		fd_set exceptionfds ; FD_ZERO(&exceptionfds);
		int fd_index = 0;
		int fd_client = 0 ;
		int change = 0;

		FD_SET(server, &readfds);
		FD_SET(server, &exceptionfds);
		for (fd_index = 0 ; fd_index < BACKLOG; fd_index++) {
			if (isconnect[fd_index]) {
				fd_client = client_start + fd_index ;
				FD_SET(fd_client, &readfds);
				FD_SET(fd_client, &exceptionfds);
				if (fd_client > maxfd)
					maxfd = fd_client;
			}
		}
		if (change = select(maxfd + 1, &readfds, NULL, &exceptionfds, NULL), change > 0) {
			int fd = 0 ;
			for ( fd = server; fd <= maxfd && change > 0; fd++) {

				/* 	接收带外数据 !!
					如果不设置 SIGURG 也不设置 异常集合 也不设置 SO_OOBINLINE 的话
					那就一直接收不到 带外数据

					带外标记的以下两个特性：

					1. "带外标记"总是指向普通数据最后一个字节紧后的位置,这意味着
						sockatmask:
						a.开启SO_OOBINLINE (带外数据在线接收)
							那么如果下一个待读入的字节时使用MSG_OOB标志发送的,sockatmask就返回真
						b.没有开启SO_OOBINLINE套接字
							那么若下一个待读入的字节是跟在带外数据后发送的第一个字节,sockatmark就返回真。

					2. 读操作总是停在"带外标记"上
						也就是说，如果在套接字接收缓冲区有100个字节,不过在带外标记之前只有5个字节
						而接收进程执行一个请求100个字节的read调用,那么返回是带外标记之前的5个字节

						这种在带外标记上强制停止读操作的做法
						使得进程能够调用sockatmark确实缓冲区指针是否处于带外标记

					另外两个特性:
					3.即使因为流量控制而停止发送数据，TCP仍然发送带外数据的通知（即它的紧急指针）
					4.在带外数据到达之前
						接收进程可能被通知说发送进程已经发送了带外数据(使用SIGURG信号或通过select)
						如果接收进程接着指定MSG_OOB调用recv,而带外数据却尚未到达
						recv将返回EWOULDBLOCK/Resource temporarily unavailable错误

						e.g 发送端协议栈发送buffer比较大 但是 接收端协议栈接收buffer很少
							接受处理程序 不能很快从 协议栈接收buffer 读数据 导致 带外数据还在发送端协议栈buffer
							但是
								发送端TCP向接收端TCP发送了带外通知(特性3)，由此产生递交给接收进程的SIGURG信号。
							然而当接收进程指定MSG_OOB标志调用recv时，相应带外字节不能读入因为带外数据还没有到达

						解决办法是让接收进程通知读入已排队的普通数据，在套接字接收缓冲区中腾出空间
						这将导致接收端TCP向发送端通告一个非零的窗口，最终允许发送带外字节

					5. 一个给定TCP连接只有一个"带外标记" 如果在接收进程读入某个现有带外数据之前
						有新的带外数据到达,先前的标记就丢失
				 */
		        if (FD_ISSET(fd, &exceptionfds)) {
		        	change--;

		        	int mark1 = sockatmark(fd); // 实现就是 ioctl SIOCATMARK
		        	int mark2 = 0 ;
		        	ioctl(fd,SIOCATMARK, &mark2 , sizeof(mark2));
		        	printf("sockatmark %d SIOCATMARK %d \n" , mark1, mark2);
#if 1
		        	/* 如果启用了exception fds
		        	 * 在不处理接收带外数据之前,(SO_OOBINLINE使用普通read 不启用OOBINLINE使用OOB_MSG接收)
		        	 * 会不断地产生异常 及exception fds不断从select中有效
		        	 * */
					int oob_len = recv(fd, rbuf, sizeof(rbuf)-1, MSG_OOB);
					if( oob_len < 0 ){
						printf("exception : recv MSG_OOB error %d %s from %d \n",errno,strerror(errno),fd);
					}else{
						rbuf[oob_len] = 0;
						printf("exception : read %d OOB byte: %s from %d \n", oob_len, rbuf , fd);
					}
#endif
					/**
					 *  OOB_MSG返回错误 22 Invalid argument
					 *
					 *  a. 如果打开了SO_OOBINLINE 带外数据放入正常流 再用MSG_OOB接收就会错误22
					 * 	b. 或这如果已经在excpetionfds中用OOB_MSG接收了 带外数据  后续再用OOB_MSG接收就会错误22
					 *
					 * 	sockatmark 0 SIOCATMARK 0
						exception : recv MSG_OOB error 22 Invalid argument <= 异常fds还是可以用,但是用recv MSG_OOB来接收会出错
						recv from 4 : 13 , ABCDEFGHIJKLM					<= 读操作总是停在带外标记

						sockatmark 1 SIOCATMARK 1							<= 定位到下个read返回的第一个字节是OOB数据
						exception : recv MSG_OOB error 22 Invalid argument  <= 异常fds产生了两次
						recv from 4 : 1 , N									<= 接收到oob后面的数据
					 *
					 */


		        }


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

					} else {

						/*
						 * 如果不设置SO_OOBINLINE sockatmark返回ture代表指向带外数据下一个字节
						 * 所以即使接收了一个OOB 也要后面有发送端发送其他数据过来 才能判断出
						 *
						 * 暗示了如果不用exception fds处理接收的话
						 * 这里就应该判断出atmark后使用recv OOB_MSG接收
						 *
						 * 但是如果exceptin中读取了"带外字节" 就不用在这里rece MSG_OOB
						 * 否则会遇上错误 "22 Invalid argument"
						 *
						 * 如果设置SO_OOBINLINE "带外数据放入正常流中"
			        	 * sockatmark "暗示下一个read返回的 第一个字节" 是"带外字节"
			        	 * 但是 SO_OOBINLINE "不需要MSG_OOB标记来接收"(错误22 Invalid argument)
			        	 *
						 */
			      		int mark1 = sockatmark(fd);
						printf("next atmark %d \n" ,mark1);
						if( mark1 == 1 ){
			        		int oob_len = recv(fd, rbuf, sizeof(rbuf)-1, MSG_OOB);
			        		if( oob_len < 0 ){
			        			printf("NOT SO_OOBINLINE but no exception handled : recv MSG_OOB error %d %s from %d \n",errno,strerror(errno),fd);
			        		}else{
				        		rbuf[oob_len] = 0;
				        		printf("NOT SO_OOBINLINE but no exception handled : read %d OOB byte: %s from %d \n", oob_len, rbuf , fd);
			        		}
						}

						int read_size = 0;
						bzero(rbuf, sizeof(rbuf));
						if (read_size = read(fd, rbuf, sizeof(rbuf) - 1), read_size
								> 0) {
							rbuf[read_size] = '\0';
							printf("recv from %d : %d , %s\n", fd, read_size,rbuf);
							if (write(fd, "ok", sizeof("ok")) <= 0) {
								printf("write error %d %s\n", errno , strerror(errno));
								if(errno == EPIPE){
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
						else if(read_size == 0 ){
							printf("peer close %d %s \n", errno ,strerror(errno));
							isconnect[fd - client_start] = 0;
							FD_CLR(fd, &readfds);
							close(fd);
							continue;
						}
						else {
							if (errno == ECONNRESET) {
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
