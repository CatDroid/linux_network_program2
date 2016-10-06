/*open-01.c打开文件的例子*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
int main(void)
{
  int fd = -1;									/*文件描述符声明*/
  char filename[] = "test.txt";					/*打开的文件名*/
  fd = open(filename,O_RDWR);					/*打开文件为可读写方式*/
  if(-1 == fd){									/*打开失败*/      
	  printf("Open file %s failure!, fd:%d\n",filename,fd);
	}	else	{	  							/*打开成功*/
	  printf("Open file %s success,fd:%d\n",filename,fd);
	}
	return 0;
}
