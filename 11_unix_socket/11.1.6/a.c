#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

ssize_t send_fd(int fd, void*data, size_t bytes, int sendfd)
{
	struct msghdr msghdr_send;				/*发送消息*/
	struct iovec iov[1];					/*向量*/
	/*方便操作msg的结构*/
	union{
		struct cmsghdr cm;					/*control msg结构*/
		char control[CMSG_SPACE(sizeof(int))];
									/*字符指针，方便控制*/
	}control_un;
	struct cmsghdr*pcmsghdr=NULL;			/*控制头部的指针*/
	msghdr_send.msg_control = control_un.control;	/*控制消息*/
	msghdr_send.msg_controllen = sizeof(control_un.control);
									/*长度*/
	
	pcmsghdr = CMSG_FIRSTHDR(&msghdr_send);	/*取得第一个消息头*/
	pcmsghdr->cmsg_len = CMSG_LEN(sizeof(int));	/*获得长度*/
	pcmsghdr->cmsg_level = SOL_SOCKET;			/*用于控制消息*/
	pcmsghdr->cmsg_type = SCM_RIGHTS;
	*((int*)CMSG_DATA(pcmsghdr))= sendfd;		/*socket值*/
	
	
	msghdr_send.msg_name = NULL;				/*名称*/
	msghdr_send.msg_namelen = 0;				/*名称长度*/
	
	iov[0].iov_base = data;						/*向量指针*/
	iov[0].iov_len = bytes;						/*数据长度*/
	msghdr_send.msg_iov = iov;					/*填充消息*/
	msghdr_send.msg_iovlen = 1;
	
	return (sendmsg(fd, &msghdr_send, 0));		/*发送消息*/
}

int main(int argc, char*argv[])
{
	int fd;
	ssize_t n;
	
	if(argc != 4)
		printf("socketpair error\n");
	if((fd = open(argv[2],atoi(argv[3])))<0) /*打开输入的文件名称*/
		return 0;
		
	if((n =send_fd(atoi(argv[1]),"",1,fd))<0)	/*发送文件描述符*/
		return 0;
	return 0;
}