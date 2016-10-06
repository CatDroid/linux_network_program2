/*
* pthread.c
* 线程实例
*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
static int run = 1;								/*运行状态参数*/
static int retvalue ;							/*线程返回值*/
void *start_routine(void *arg)					/*线程处理函数*/
{
		int *running = arg;						/*获取运行状态指针*/
		printf("子线程初始化完毕，传入参数为:%d\n",*running); /*打印信息*/
		while(*running)							/*当running控制参数有效*/
		{
			printf("子线程正在运行\n");			/*打印运行信息*/
			usleep(1);							/*等待*/
		}
		printf("子线程退出\n");					/*打印退出信息*/
		
		retvalue = 8;							/*设置退出值*/
		pthread_exit( (void*)&retvalue);		/*线程退出并设置退出值*/
}
int main(void)
{
	pthread_t pt;
	int ret = -1;
	int times = 3;
	int i = 0; 
	int *ret_join = NULL;
	
	ret = pthread_create(&pt, NULL, (void*)start_routine, &run);													/*建立线程*/
	if(ret != 0)							/*建立线程失败*/
	{
		printf("建立线程失败\n");				/*打印信息*/
		return 1;							/*返回*/
	}	
	usleep(1);								/*等待*/
	for(;i<times;i++)						/*进行3次打印*/
	{
		printf("主线程打印\n");				/*打印信息*/
		usleep(1);							/*等待*/
	}
	run = 0;								/*设置线程退出控制值，让线程退出*/
	pthread_join(pt,(void*)&ret_join);		/*等待线程退出*/
	printf("线程返回值为:%d\n",*ret_join);	/*打印线程的退出值*/
	return 0;
}