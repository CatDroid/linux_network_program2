/*
* mutex.c
* 线程实例
*/
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
void *producter_f (void *arg);					/*生产者*/
void *consumer_f (void *arg);					/*消费者*/
int buffer_has_item=0;							/*缓冲区计数值*/
pthread_mutex_t mutex;							/*互斥区*/
int running =1 ;								/*线程运行控制*/
int main (void)
{
	pthread_t consumer_t;						/*消费者线程参数*/
	pthread_t producter_t;						/*生产者线程参数*/
	
	pthread_mutex_init (&mutex,NULL);			/*初始化互斥*/
	
	pthread_create(&producter_t, NULL,(void*)producter_f, NULL );														/*建立生产者线程*/
	pthread_create(&consumer_t, NULL, (void *)consumer_f, NULL);														/*建立消费者线程*/
	usleep(1);									/*等待，线程创建完毕*/
	running =0;									/*设置线程退出值*/
	pthread_join(consumer_t,NULL);				/*等待消费者线程退出*/
	pthread_join(producter_t,NULL);			/*等待生产者线程退出*/
	pthread_mutex_destroy(&mutex);				/*销毁互斥*/
	
	return 0;
}
void *producter_f (void *arg)					/*生产者线程程序*/
{
	while(running)								/*没有设置退出值*/
	{	
		pthread_mutex_lock (&mutex);			/*进入互斥区*/
		buffer_has_item++;						/*增加计数值*/
		printf("生产，总数量:%d\n",buffer_has_item); 	/*打印信息*/
		pthread_mutex_unlock(&mutex);			/*离开互斥区*/
	}
}
void *consumer_f(void *arg)						/*消费者线程程序*/
{
	while(running)								/*没有设置退出值*/
	{
		pthread_mutex_lock(&mutex);				/*进入互斥区*/
		buffer_has_item--;						/*减小计数值*/
		printf("消费，总数量:%d\n",buffer_has_item);	/*打印信息*/
		pthread_mutex_unlock(&mutex);			/*离开互斥区*/
	}
}