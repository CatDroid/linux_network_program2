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
#define UNIX_PATH "/tmp/unix_socket_file"
#define SHARD_FILE_PATH "/tmp/unix_socket_shared_file"

static void sigint_handler(int signo) {
	printf("catch signal!\n");
	return;
}

ssize_t send_fd(int fd, int file_fd , struct sockaddr* target , int target_len);

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
	char rbuf[256];
	char wbuf[256];

	int shared_file = -1 ;
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

			if( count > sizeof(rbuf) - 1){
				printf("消息过大 已被截断\n");
				rbuf[sizeof(rbuf)-1] = '\0';
			}else if( count > 0 ) {
				rbuf[count] = '\0' ;
			}else if( count <= 0 ){
				printf("recvfrom error %d %s\n", errno , strerror(errno));
				continue ;
			}

#define OPEN  		"open"
#define SEEK_START  "seek_start"
#define CLOSE 		"close"

			if( from_len != 0){
				int sendsize  = -1 ;

				if( ! strncmp(rbuf, OPEN, strlen(OPEN)) ){
					if(shared_file == -1){
						shared_file = open(SHARD_FILE_PATH, O_RDWR|O_CREAT , 0770);
						if(shared_file < 0){
							printf("open fail %d %s !\n",errno,strerror(errno));
							shared_file = -1 ;
						}
					}
					/*	如果sendmsg发送错误的文件描述符 将会遇到:
					 * 	open fail 13 Permission denied !
						sent = -1 9 Bad file descriptor
						sendto error ! sendsize = -1 errno = 9 Bad file descriptor
						而且另外一段也接收不到(  to do ..这种情况应该 也要返回 一些信息给对端 )

					 *
					 * */
					sendsize = send_fd(sock_UNIX , shared_file , (struct sockaddr*)&from_addr , from_len);
				}else if( ! strncmp(rbuf, SEEK_START, strlen(SEEK_START)) ){
					if(shared_file != -1){
						lseek(shared_file,0,SEEK_SET);
					}
					sendsize = sendto(sock_UNIX, "ok", sizeof("ok"), 0, (struct sockaddr*)&from_addr,  from_len );
				}else if( ! strncmp(rbuf, CLOSE, strlen(CLOSE)) ){
					if(shared_file != -1){
						close(shared_file);
						shared_file = -1 ;
					}
					sendsize = sendto(sock_UNIX, "ok", sizeof("ok"),
							0, (struct sockaddr*)&from_addr,  from_len );
				}else{
					sendsize = sendto(sock_UNIX, "unknown", sizeof("unknown"), 0, (struct sockaddr*)&from_addr,  from_len );
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

ssize_t send_fd(int fd, int file_fd , struct sockaddr* target , int target_len)
{
	// 使用sendmsg发送数据(这里只填充msg_control中的一个cmsghdr)
	struct msghdr msghdr_send;

	// 	CMSG_SPACE(辅助数据大小)得到总长度  (有对齐辅助数据大小)
	//  CMSG_LEN(辅助数据大小)  得到总长度  (没有对其辅助数据大小)
	// 	CMSG_DATA(&csmghdr) 获得指向存放辅助数据的位置
	//  CMSG_NXTHDR(&msghdr,&) 	下一个头部
	//  CMSG_FIRSTHDR(&msghdr)	第一个头部
	union{
		struct cmsghdr cm;
		//CMSG_SPACE计算空间= cmsghdr的大小 + sizeof(int)(用来保存文件描述符的)
		char control[CMSG_SPACE(sizeof(int))];
	}control_un;

	// msghdr的msg_control部分/辅助数据
	msghdr_send.msg_control = control_un.control;
	msghdr_send.msg_controllen = sizeof(control_un.control);

	// 初始化 struct cmsghdr(辅助数据)
	struct cmsghdr*pcmsghdr=NULL;
	pcmsghdr = CMSG_FIRSTHDR(&msghdr_send);		// 从struct msghdr(非cmsghdr)取得第一个消息头
	pcmsghdr->cmsg_len = CMSG_LEN(sizeof(int));
	pcmsghdr->cmsg_level = SOL_SOCKET;
	pcmsghdr->cmsg_type = SCM_RIGHTS;			// 代表传输文件描述符 Transfer file descriptors
	*((int*)CMSG_DATA(pcmsghdr))= file_fd;		// 传递的文件描述符  放在 sizeof(int)的位置

	// msghdr的msg_name和msg_namelen部分/目标地址 sendto 一样
	msghdr_send.msg_name = target;				/* Address to send to/receive from.  */
	msghdr_send.msg_namelen = target_len;		/* Length of address data.  */

	// msghdr的msg_iov部分/接收发送缓冲区
	//struct iovec iov[1];
	//iov[0].iov_base = "ok";
	//iov[0].iov_len = strlen("ok");
	//msghdr_send.msg_iov = iov;
	//msghdr_send.msg_iovlen = 1;
	msghdr_send.msg_iov = NULL;
	msghdr_send.msg_iovlen = 0;

	// sendmsg 和 recvmsg返回的是 所有msg_iov写入或者接受的字节大小
	int sent = sendmsg(fd, &msghdr_send, 0);
	if(sent < 0){
		printf("sent error! %d %d %s\n", sent, errno, strerror(errno) );
	}else{
		printf("sent done! %d \n", sent );
	}

	return sent ;
}

