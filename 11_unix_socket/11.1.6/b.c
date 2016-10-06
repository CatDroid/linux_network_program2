#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
*	从fd中接收消息，并将文件描述符放在指针recvfd中
*/
ssize_t recv_fd(int fd, void*data, size_t bytes, int*recvfd)
{
	struct msghdr msghdr_recv;					/*接收消息接收*/
	struct iovec iov[1];						/*接收数据的向量*/
	size_t n;
	
	union{
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];	
	}control_un;
	struct cmsghdr*pcmsghdr;						/*消息头部*/
	msghdr_recv.msg_control = control_un.control;	/*控制消息*/
	msghdr_recv.msg_controllen = sizeof(control_un.control);	
											/*控制消息的长度*/
	
	msghdr_recv.msg_name = NULL;	/*消息的名称为空*/
	msghdr_recv.msg_namelen = 0;	/*消息的长度为空*/
	
	iov[0].iov_base = data;			/*向量的数据为传入的数据*/
	iov[0].iov_len = bytes;			/*向量的长度为传入数据的长度*/
	msghdr_recv.msg_iov = iov;		/*消息向量指针*/
	msghdr_recv.msg_iovlen = 1;		/*消息向量的个数为1个*/
	if((n = recvmsg(fd, &msghdr_recv, 0))<=0)	/*接收消息*/
		return n;
		
	if((pcmsghdr = CMSG_FIRSTHDR(&msghdr_recv))!= NULL &&
											/*获得消息的头部*/
pcmsghdr->cmsg_len == CMSG_LEN(sizeof(int))){	
											/*获得消息的长度为int*/
		if(pcmsghdr->cmsg_level != SOL_SOCKET)
											/*消息的level应该为SOL_SOCKET*/
			printf("control level != SOL_SOCKET\n");
		
		if(pcmsghdr->cmsg_type != SCM_RIGHTS)	/*消息的类型判断*/
			printf("control type != SCM_RIGHTS\n");
			
		*recvfd =*((int*)CMSG_DATA(pcmsghdr));
						/*获得打开文件的描述符*/
	}else
		*recvfd = -1;
		
	return n;						/*返回接收消息的长度*/
}

int my_open(const char*pathname, int mode)
{
	int fd, sockfd[2],status;
	pid_t childpid;
	char c, argsockfd[10],argmode[10];
	
	socketpair(AF_LOCAL,SOCK_STREAM,0,sockfd);	/*建立socket*/
	if((childpid = fork())==0){					/*子进程*/
		close(sockfd[0]);						/*关闭sockfd[0]*/
		snprintf(argsockfd, sizeof(argsockfd),"%d",sockfd[1]);															/*socket描述符*/
		snprintf(argmode, sizeof(argmode),"%d",mode);
														/*打开文件的方式*/
		execl("./openfile","openfile",argsockfd, pathname,argmode,(char*)NULL)					;/*执行进程A*/
		printf("execl error\n");
	}	
	/*父进程*/
	close(sockfd[1]);
	/*等待子进程结束*/
	waitpid(childpid, &status,0);
	
	if(WIFEXITED(status)==0)				/*判断子进程是否结束*/
		printf("child did not terminate\n")	;
	if((status = WEXITSTATUS(status))==0){	/*子进程结束*/
		recv_fd(sockfd[0],&c,1,&fd);	/*接收进程A打开的文件描述符*/
	}else{
		errno = status;
		fd = -1;	
	}	
	
	close(sockfd[0]);					/*关闭sockfd[0]*/
	return fd;							/*返回进程A打开文件的描述符*/

}

#define BUFFSIZE 256					/*接收的缓冲区大小*/
int main(int argc, char*argv[])
{
	int fd, n;
	char buff[BUFFSIZE];				/*接收缓冲区*/
	
	if(argc !=2)
		printf("error argc\n");
		
	if((fd = my_open(argv[1], O_RDONLY))<0)
								/*获得进程A打开的文件描述符*/
		printf("can't open %s\n",argv[1]);
	
	while((n = read(fd, buff, BUFFSIZE))>0)	/*读取数据*/
	write(1,buff,n);							/*写入标准输出*/
	
	return(0);	
}