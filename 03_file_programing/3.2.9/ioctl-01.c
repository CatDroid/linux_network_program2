/*文件ioctl-01.c控制CDROM*/
#include <Linux/cdrom.h>
#include <stdio.h>
#include <fcntl.h>
int main(void){
	
	int fd = open("/dev/cdrom",O_RDONLY); 			/*打开CDROM设备文件*/
	if(fd < 0){
		printf("打开CDROM失败\n");
		return -1;
	}
	/*向Linux内核的CDROM驱动程序发送CDROMEJECT 请求*/
	if (!ioctl(fd, CDROMEJECT,NULL)){				/*驱动程序操作成功*/
		printf("成功弹出CDROM\n");
	}else{											/*驱动程序操作失败*/	
		printf("弹出CDROM失败\n");
	}
	
	close(fd); 										/*关闭CDROM设备文件*/
	return 0;
}