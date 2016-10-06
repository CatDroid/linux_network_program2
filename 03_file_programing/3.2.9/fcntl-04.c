/*文件fcntl-04.c,使用fcntl获得接收信号的进程ID*/
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
int main(void){
	int uid;
	int fd = open("test.txt", O_RDWR); 		/*打开文件test.txt*/
	
	uid = fcntl(fd, F_GETOWN); 				/*获得接收信号的进程ID*/
	printf("the SIG recv ID is %d\n",uid);
	
	close(fd);
	return 0;
}