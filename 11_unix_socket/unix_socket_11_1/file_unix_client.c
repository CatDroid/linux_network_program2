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

#define SERVER_UNIX_PATH "/tmp/unix_socket_file"
#define REFUSED "refuse\n"
#define USING_ABSTRACT_ADDRESS 0


ssize_t recv_fd(int fd , int*recvfd);


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
#if USING_ABSTRACT_ADDRESS
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

#define OPEN  		"open"
#define SEEK_START  "seek_start"
#define CLOSE 		"close"
#define WRITE		"write"
#define EXIT		"exit"
	int shared_fd = -1 ;
	while (1) {

		bzero(wbuf, sizeof(wbuf));
		printf("please enter information:\n");
		fgets(wbuf, sizeof(wbuf), stdin);


		if( !strncmp(wbuf, OPEN ,strlen(OPEN)) ){
			if(write(sock, OPEN, strlen(OPEN)) <= 0) {
				perror("write error");
				break;
			}
			recv_fd( sock , &shared_fd);
			if(shared_fd != -1){
				printf("open done!\n");
			}else{
				printf("open fail!\n");
			}
			continue ;// do NOT need server reply 'ok'
		}else if( !strncmp(wbuf, SEEK_START ,strlen(SEEK_START)) ){
			if(shared_fd != -1){
				if(write(sock, SEEK_START, strlen(SEEK_START)) <= 0) {
					perror("write error");
					break;
				}
			}else{
				printf("seek none ! OPEN first!\n");
				continue ;
			}
		}else if( !strncmp(wbuf, CLOSE ,strlen(CLOSE)) ){
			if(shared_fd != -1){
				if(write(sock, CLOSE, strlen(CLOSE)) <= 0) {
						perror("write error");
						break;
				}
				write(shared_fd,"write after server closed",strlen("write after server closed"));
				close(shared_fd);
				shared_fd = -1 ;
			}else{
				printf("close none ! OPEN first!\n");
				continue ;
			}
		}else if( !strncmp(wbuf, WRITE ,strlen(WRITE)) ){//write1234790
			if(shared_fd != -1){
				write(shared_fd , wbuf+strlen(WRITE), strlen(wbuf+strlen(WRITE)) );
				printf("write done\n");
			}else{
				printf("write none ! OPEN first!\n");
			}
			continue ;
		}else if (!strncmp(wbuf, EXIT , strlen(EXIT))) {
			if(shared_fd != -1){
				if(write(sock, CLOSE, strlen(CLOSE)) <= 0) {
					perror("write error");
					break;
				}
				close(shared_fd);
				shared_fd = -1 ;
			}
			printf("This process is about to exit....\n");
			break;
		}else{
			printf("unknown command %s\n" , wbuf);
			continue ;
		}

		// wait-for-server-reply-ok
		bzero(rbuf, sizeof(rbuf));
		if (read(sock, rbuf, sizeof(rbuf)) > 0) {
			if (!strncmp(rbuf, "ok", 2)) {
				printf("server recvice data\n");
				printf("ack %s\n", rbuf);
			} else {
				printf("server ack non-ok %s\n", rbuf);
			}
		} else {
			printf("read error %d %s \n" , errno , strerror(errno));
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


ssize_t recv_fd(int fd , int*recvfd)
{
	struct msghdr msghdr_recv;
	struct iovec iov[1];
	size_t n;

	union{
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	}control_un;
	struct cmsghdr* pcmsghdr;
	msghdr_recv.msg_control = control_un.control;
	msghdr_recv.msg_controllen = sizeof(control_un.control);

	struct sockaddr_un from_addr;
	memset(&from_addr, 0 ,sizeof(struct sockaddr_un));
	int from_len = sizeof(struct sockaddr_un);

	msghdr_recv.msg_name = &from_addr; // 内核会把对段的地址填充到这里 就响recvfrom
	msghdr_recv.msg_namelen = from_len;

//	unsigned char iov_buff[16];
//	iov[0].iov_base = iov_buff;
//	iov[0].iov_len = sizeof(iov_buff);
//	msghdr_recv.msg_iov = iov;
//	msghdr_recv.msg_iovlen = 1;
	msghdr_recv.msg_iov = NULL;
	msghdr_recv.msg_iovlen = 0;

	n = recvmsg(fd, &msghdr_recv, 0) ;
	if(n < 0 ){
		printf("recvmsg error %zd %d %s\n" , n , errno , strerror(errno) );
		*recvfd = -1;
		return n;
	}
	printf("revvmsg %zd from [%c%c%c%c%c%c%c%c%c%c]\n" , n ,
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

	if((pcmsghdr = CMSG_FIRSTHDR(&msghdr_recv))!= NULL &&
			pcmsghdr->cmsg_len == CMSG_LEN(sizeof(int))){

		if(pcmsghdr->cmsg_level != SOL_SOCKET){
			printf("control level != SOL_SOCKET\n");
			*recvfd = -1;
			return n;
		}

		if(pcmsghdr->cmsg_type != SCM_RIGHTS){
			printf("control type != SCM_RIGHTS\n");
			*recvfd = -1;
			return n;
		}
		*recvfd =*((int*)CMSG_DATA(pcmsghdr));
	}else{
		*recvfd = -1;
	}

	return n;
}




