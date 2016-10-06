/*文件lseek-01.c,使用lseek函数测试标准输入是否可以进行seek操作*/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
int main(void)
{
	off_t offset = -1;	
	
	
	offset = lseek(1, 0, SEEK_CUR); 	/*将标准输入文件描述符的文件偏移量												 设为当前值*/
	if(-1 == offset){ 						/*设置失败，标准输入不能进行seek																 操作*/		
		printf("STDIN can't seek\n");
		return -1;
	}else{								/*设置成功，标准输入可以进行seek操作*/
		printf("STDIN CAN seek\n");	
	};
	return 0;
}
