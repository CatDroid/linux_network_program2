/*文件fcntl-05.c,使用fcntl设置接收信号的进程ID*/
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
int main(void){
	int uid;
	int fd = open("test.txt", O_RDWR); 		/*打开文件test.txt*/
	
	uid = fcntl(fd, F_SETOWN,10000);			/*设置接收信号的进程ID*/

	close(fd);
	return 0;
}