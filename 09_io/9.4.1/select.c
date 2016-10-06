#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
int main(void) {
	fd_set rd;							/*读文件集和*/
	struct timeval tv;					/*时间间隔*/
	int err;								/*错误值*/
											/*监视标准输入是否可以读数据*/
	FD_ZERO(&rd);
	FD_SET(0, &rd);
											/*设置5s的等待超时*/
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	err = select(1, &rd, NULL, NULL, &tv);
											/*函数返回，查看返回条件*/
	if (err == -1)						/*出错*/
		perror("select()");
	else if (err)						/*标准输入有数据输入，可读*/
		printf("Data is available now.\n");
										/*FD_ISSET(0, & rd) 的值为真*/
	else
		printf("No data within five seconds.\n");/*超时，没有数据到达*/
	return 0;
}