/*
* sem.c
* 线程实例
*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
void *producter_f (void *arg);					/*生产者线程函数*/
void *consumer_f (void *arg);					/*消费者线程函数*/
sem_t sem;
int running =1 ;
int main (void)
{
	pthread_t consumer_t;						/*消费者线程参数*/
	pthread_t producter_t;						/*生产者线程参数*/
	
	sem_init (&sem, 0, 16);						/*信号量初始化*/
	
	pthread_create(&producter_t, NULL,(void*)producter_f, NULL ); 														/*建立生产者线程*/
	pthread_create(&consumer_t, NULL, (void *)consumer_f, NULL); 														/*建立消费者线程*/
	sleep(1);									/*等待*/
	running =0;									/*设置线程退出*/
	pthread_join(consumer_t,NULL);				/*等待消费者线程退出*/
	pthread_join(producter_t,NULL);			/*等待生产者线程退出*/
	sem_destroy(&sem);							/*销毁信号量*/
	
	return 0;
}
void *producter_f (void *arg)					/*生产者处理程序代码*/
{
	int semval=0;								/*信号量的初始值为0*/
	while(running)								/*运行状态为可运行*/
	{
		usleep(1);								/*等待*/
		sem_post (&sem);						/*信号量增加*/
		sem_getvalue(&sem,&semval);				/*获得信号量的值*/
		printf("生产，总数量:%d\n",semval);		/*打印信息*/
	}
}
void *consumer_f(void *arg)						/*消费者处理程序代码*/
{
	int semval=0;								/*信号量的初始值为0*/
	while(running)								/*运行状态为可运行*/
	{
		usleep(1);								/*等待*/
		sem_wait(&sem);							/*等待信号量*/
		sem_getvalue(&sem,&semval);				/*获得信号量的值*/
		printf("消费，总数量:%d\n",semval);		/*打印信息*/
	}
}