#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
int main(void)
{
	int result = -1;						/*创建管道结果*/
	int fd[2];						/*文件描述符，字符个数*/
	pid_t pid;								/*PID值*/
	/* 文件描述符1用于写，文件描述符0用于读 */
	int *write_fd = &fd[1];					/*写文件描述符*/
	int *read_fd = &fd[0];					/*读文件描述符*/
	
	result = pipe(fd);						/*建立管道*/
	if( -1 == result)						/*建立管道失败*/
	{
		printf("建立管道失败\n");				/*打印信息*/
		return -1;							/*返回错误结果*/
	}
	
	pid = fork();							/*分叉程序*/
	if( -1 == pid)							/*fork失败*/
	{
		printf("fork 进程失败\n");			/*打印信息*/
		return -1;					/*返回错误结果*/
	}
	
	if( 0 == pid) 					/*子进程*/
	{
		close(*read_fd);			/*关闭读端*/
	}
	else							/*父进程*/
	{
		close(*write_fd);			/*关闭写端*/
	}
	
	return 0;
}