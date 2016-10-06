#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>

int issockettype(int fd)
{
	struct stat st;
	int err =	fstat(fd, &st);						/*获得文件的状态*/
	
	if( err < 0 )	{
		return -1;		
	}
	
	if((st.st_mode & S_IFMT) == S_IFSOCK)	{		/*比较是否套接字描述														*/
		return 1;
	}	else{
		return 0;
	}
}
int main(void)
{
	int ret = issockettype(0);				/*查询标准输入是否套接字描述符												*/
	printf("value %d\n",ret);
	
	int s = socket(AF_INET, SOCK_STREAM,0);	/*建立套接字描述*/
	ret = issockettype(s);						/*查询是否为套接字描述*/
	printf("value %d\n",ret);					/*输出结果*/
	
	return 0;
}
