#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/*
*错误处理函数
*/
static void display_err(const char*on_what)
{
	perror(on_what);
	exit(1);
}

int main(int argc,char*argv[])
{
	int error;	 							/*错误值*/
	int sock_UNIX;							/*socket*/
	struct sockaddr_un addr_UNIX;			/*AF_UNIX族地址*/
	int len_UNIX;							/*AF_UNIX族地址长度*/
	const char path[] = "/demon/path";		/*路径名*/

	/*
	*建立套接字
	*/
	sock_UNIX = socket(AF_UNIX,SOCK_STREAM,0);
	
	if(sock_UNIX == -1)
		display_err("socket()");

	/*
	*由于之前已经使用了path路径用于其他用途
	*需要将之前的绑定取消
	*/
	unlink(path);

	/*
	*填充地址结构
	*/
	memset(&addr_UNIX,0,sizeof(addr_UNIX));

	addr_UNIX.sun_family = AF_LOCAL;
	strcpy(addr_UNIX.sun_path,path);
	len_UNIX = sizeof(struct sockaddr_un);

	/*
	*绑定地址到socket sock_UNIX
	*/
	error = bind(sock_UNIX,
			(struct sockaddr*)&addr_UNIX,
			len_UNIX);
	if(error == -1)
		display_err("bind()");
	
	/*
	*关闭socket
	*/
	close(sock_UNIX);
	unlink(path);

	return 0;
}