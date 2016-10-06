#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
void msg_show_attr(int msg_id, struct msqid_ds msg_info)																	/*打印消息属性的函数*/
{
	int ret = -1;
	sleep(1);
	ret = msgctl(msg_id, IPC_STAT, &msg_info);		/*获取消息*/
	if( -1 == ret)
	{
		printf("获得消息信息失败\n");					/*获取消息失败，返回*/
		return ;		
	}
	
	printf("\n");									/*以下打印消息的信息*/
	printf("现在队列中的字节数：%ld\n",msg_info.msg_cbytes);																	/*消息队列中的字节数*/
	printf("队列中消息数：%d\n",(int)msg_info.msg_qnum);	/*消息队列中的消息数*/
	printf("队列中最大字节数：%d\n",(int)msg_info.msg_qbytes);																	/*消息队列中的最大字节数*/
	printf("最后发送消息的进程pid：%d\n",msg_info.msg_lspid);																/*最后发送消息的进程*/
	printf("最后接收消息的进程pid：%d\n",msg_info.msg_lrpid);																/*最后接收消息的进程*/
	printf("最后发送消息的时间：%s",ctime(&(msg_info.msg_stime)));														/*最后发送消息的时间*/
	printf("最后接收消息的时间：%s",ctime(&(msg_info.msg_rtime)));														/*最后接收消息的时间*/
	printf("最后变化时间：%s",ctime(&(msg_info.msg_ctime)));																/*消息的最后变化时间*/
	printf("消息UID是：%d\n",msg_info.msg_perm.uid);	/*消息的UID*/
	printf("消息GID是：%d\n",msg_info.msg_perm.gid);	/*消息的GID*/
}
int main(void)
{
	int ret = -1;
	int msg_flags, msg_id;
	key_t key;
	struct msgmbuf{								/*消息的缓冲区结构*/
		int mtype;
		char mtext[10];
		};	
	struct msqid_ds msg_info;
	struct msgmbuf msg_mbuf;
	
	int msg_sflags,msg_rflags;
	char *msgpath = "/ipc/msg/";				/*消息key产生所用的路径*/
	key = ftok(msgpath,'b');					/*产生key*/
	if(key != -1)								/*产生key成功*/
	{
		printf("成功建立KEY\n");		
	}
	else										/*产生key失败*/
	{
		printf("建立KEY失败\n");		
	}
	
	msg_flags = IPC_CREAT|IPC_EXCL;			/*消息的类型*/
	msg_id = msgget(key, msg_flags|0x0666);	/*建立消息*/
	if( -1 == msg_id)
	{
		printf("消息建立失败\n");
		return 0;		
	}	
	msg_show_attr(msg_id, msg_info);			/*显示消息的属性*/
	
	msg_sflags = IPC_NOWAIT;
	msg_mbuf.mtype = 10;
	memcpy(msg_mbuf.mtext,"测试消息",sizeof("测试消息"));	/*复制字符串*/
	ret = msgsnd(msg_id, &msg_mbuf, sizeof("测试消息"), msg_sflags);													/*发送消息*/
	if( -1 == ret)
	{
		printf("发送消息失败\n");		
	}
	msg_show_attr(msg_id, msg_info);			/*显示消息属性*/
	
	msg_rflags = IPC_NOWAIT|MSG_NOERROR;
	ret = msgrcv(msg_id, &msg_mbuf, 10,10,msg_rflags);		/*接收消息*/
	if( -1 == ret)
	{
		printf("接收消息失败\n");		
	}
	else
	{
		printf("接收消息成功，长度：%d\n",ret);		
	}
	msg_show_attr(msg_id, msg_info);			/*显示消息属性*/
	
	msg_info.msg_perm.uid = 8;
	msg_info.msg_perm.gid = 8;
	msg_info.msg_qbytes = 12345;
	ret = msgctl(msg_id, IPC_SET, &msg_info);	/*设置消息属性*/
	if( -1 == ret)
	{
		printf("设置消息属性失败\n");
		return 0;		
	}
	msg_show_attr(msg_id, msg_info);			/*显示消息属性*/
	
	ret = msgctl(msg_id, IPC_RMID,NULL);		/*删除消息队列*/
	if(-1 == ret)
	{
		printf("删除消息失败\n");
		return 0;		
	}
	return 0;
}
