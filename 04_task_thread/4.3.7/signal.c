#include <signal.h>
#include <stdio.h>
typedef void (*sighandler_t)(int);
static void sig_handle(int signo)			/*信号处理函数*/
{
	if( SIGSTOP== signo)					/*为SIGSTOP信号*/
	{
		printf("接收到信号SIGSTOP\n");		/*打印信息*/
	}	
	else if(SIGKILL==signo)					/*为SIGKILL信号*/
	{
		printf("接收到信号SIGKILL\n");		/*打印信息*/
	}
	else									/*其他信号*/
	{
		printf("接收到信号:%d\n",signo);		/*打印信息*/
	}
	
	return;
}
int main(void)
{
	sighandler_t ret;
	ret = signal(SIGSTOP, sig_handle);		/*挂接SIGSTOP信号处理函数*/
	if(SIG_ERR == ret)						/*挂接失败*/
	{
		printf("为SIGSTOP挂接信号处理函数失败\n");
		return -1;							/*返回*/
	}	
	
	ret = signal(SIGKILL, sig_handle);		/*挂接SIGKILL处理函数*/
	if(SIG_ERR == ret)						/*挂接失败*/
	{
		printf("为SIGKILL挂接信号处理函数失败\n");
		return -1;							/*返回*/
	}
	
	for(;;);								/*等待程序退出*/
	
}