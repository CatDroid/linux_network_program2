/*文件write-01.c,write()函数的使用*/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
int main(void)
{
	int fd = -1;
	ssize_t size = -1;
	
	char buf[]="quick brown fox jumps over the lazy dog";														/*存放数据的缓冲区*/
	char filename[] = "test.txt";
	
	fd = open(filename,O_RDWR); 		/*打开文件，如果文件不存在，则报错*/
	if(-1 == fd){ 						/*文件已经存在*/		
		printf("Open file %s failure,fd:%d\n",filename,fd);
	}	else	{							/*文件不存在，创建并打开*/		
		printf("Open file %s success,fd:%d\n",filename,fd);
	}
	
	size = write(fd, buf,strlen(buf)); 		/*将数据写入到文件test.txt中*/
	printf("write %d bytes to file %s\n",size,filename);
	
	close(fd); 								/*关闭文件*/
	return 0;
}
