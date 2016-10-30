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
	struct msghdr msghdr_send;				// 使用sendmsg发送数据
	struct iovec iov[1];					// 存放向量


	// 	CMSG_SPACE(辅助数据大小)得到总长度  (有对齐辅助数据大小)
	//  CMSG_LEN(辅助数据大小)  得到总长度  (没有对其辅助数据大小)
	// 	CMSG_DATA(&csmghdr) 获得指向存放辅助数据的位置
	//  CMSG_NXTHDR(&msghdr,&) 	下一个头部
	//  CMSG_FIRSTHDR(&msghdr)	第一个头部
	union{
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];	//CMSG_SPACE计算空间= cmsghdr的大小 + sizeof(int)(用来保存文件描述符的)
	}control_un;

	// msghdr的msg_control部分/辅助数据
	msghdr_send.msg_control = control_un.control;
	msghdr_send.msg_controllen = sizeof(control_un.control);

	// 初始化 struct cmsghdr(辅助数据)
	struct cmsghdr*pcmsghdr=NULL;
	pcmsghdr = CMSG_FIRSTHDR(&msghdr_send);		// 从struct msghdr(非cmsghdr)取得第一个消息头
	pcmsghdr->cmsg_len = CMSG_LEN(sizeof(int));
	pcmsghdr->cmsg_level = SOL_SOCKET;			//
	pcmsghdr->cmsg_type = SCM_RIGHTS;			// 代表传输文件描述符 Transfer file descriptors
	*((int*)CMSG_DATA(pcmsghdr))= sendfd;		// 传递的文件描述符  放在 sizeof(int)的位置
	
	// msghdr的msg_name和msg_namelen部分/消息名字
	msghdr_send.msg_name = NULL;
	msghdr_send.msg_namelen = 0;
	
	// msghdr的msg_iov部分/接收发送缓冲区
	iov[0].iov_base = data;						/*向量指针*/
	iov[0].iov_len = bytes;						/*数据长度*/
	msghdr_send.msg_iov = iov;					/*填充消息*/
	msghdr_send.msg_iovlen = 1;
	
	int sent = sendmsg(fd, &msghdr_send, 0);
	printf("sent = %d %d %s\n", sent, errno, strerror(errno) );
	return sent ;
}

#define WRITE_DATA "hello world"
int main(int argc, char*argv[])
{


	printf("openfile\n");
	if(argc != 4){
		printf("openfile argument error \n");
		return -1;
	}
	int fd = open( argv[2], atoi(argv[3]) );
	if(fd < 0){
		printf("open file error ! %d %s \n" , errno, strerror(errno));
		return -1;
	}
	write(fd, WRITE_DATA, strlen(WRITE_DATA) );
	int curret_pos = lseek(fd, 0, SEEK_CUR);
	printf("openfile curret_pos = %d \n" , curret_pos );
	ssize_t n =send_fd(atoi(argv[1]),"",1,fd) ;
	if(n < 0 ){
		printf("send_fd error! \n");
		return -1;
	}
	return 0;
}
