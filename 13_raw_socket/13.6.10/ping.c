/*ping.c*/ 
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h> /*bzero*/
#include <signal.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>		// 	socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <netinet/ip.h>		// 	struct ip
#include <netinet/ip_icmp.h>// 	struct icmp  ICMP_ECHO ICMP_ECHOREPLY
#include <arpa/inet.h>		// 	inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
							//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
							//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现
#include <netdb.h>			// 	getprotobyname gethostbyname
#include <net/if.h>			//  IFNAMSIZ   struct ifreq

#include <linux/sockios.h>	//  SIOCGIFADDR  /*  Socket configuration controls. */

/*保存已经发送包的状态值*/
typedef struct pingm_pakcet{
	struct timeval tv_begin;	/*发送的时间*/
	struct timeval tv_end;		/*接收到的时间*/
	short seq;					/*序列号*/
	int flag;		/*1，表示已经发送但没有接收到回应包0，表示接收到回应包*/
}pingm_pakcet;
static pingm_pakcet pingpacket[128];
static pingm_pakcet *icmp_findpacket(int seq);
static unsigned short icmp_cksum(unsigned char *data,  int len);
static struct timeval icmp_tvsub(struct timeval end,struct timeval begin);
static void icmp_statistics(void);
static void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length );
static int icmp_unpack(char *buf,int len);
static void *icmp_recv(void *argv);
static void *icmp_send(void *argv);
static void icmp_sigint(int signo);
static void icmp_usage();
#define K 1024

static char recv_buff[2*K];	/*为防止接收溢出，接收缓冲区设置大一些*/
//static char recv_buff[2]; // 按IP分组来接受  如果分片了重组后给到raw socket buffer太小只接收部分,丢弃其他
static struct sockaddr_in dest;		/*目的地址*/
static struct sockaddr_in src;		/*目的地址*/
static int rawsock = 0;					/*发送和接收线程需要的socket描述符*/
static pid_t pid=0;						/*进程PID*/
static int alive = 0;					/*是否接收到退出信号*/
static short packet_send = 0;			/*已经发送的数据包有多少*/
static short packet_recv = 0;			/*已经接收的数据包有多少*/
static char dest_str[80];				/*目的主机字符串*/
static struct timeval tv_begin, tv_end,tv_interval;
/*本程序开始发送、结束和时间间隔*/
static void icmp_usage()
{
	/*ping加IP地址或者域名*/
	printf("ping interface aaa.bbb.ccc.ddd\n");
	printf("e.g:\n");
	printf("ping eth0 192.168.1.88\n");
}
/*主程序*/
int main(int argc, char *argv[])
{
	struct hostent * host = NULL;
	struct protoent *protocol = NULL;
	int ret = 0 ;
	unsigned long inaddr = 1;
	int size = 128*K;
	/*参数是否数量正确*/
	if(argc < 3)
	{
		icmp_usage();
		return -1;
	}
	/*获取协议类型ICMP*/
	protocol = getprotobyname("icmp");
	if (protocol == NULL){
		perror("getprotobyname()");
		return -1;
	}else{
		printf("getprotobyname %s %d\n", protocol->p_name, protocol->p_proto);
	}
	/*复制目的地址字符串*/
	memcpy(dest_str,  argv[2], strlen(argv[2])+1);
	memset(pingpacket, 0, sizeof(pingm_pakcet) * 128);
	/*socket初始化*/
	rawsock = socket(AF_INET, SOCK_RAW,  protocol->p_proto);
	if(rawsock < 0){
		perror("create socket error");
		return -1;
	}
	/*为了与其他进程的ping程序区别，加入pid*/
	pid = getpid();//getuid();
	/*增大接收缓冲区，防止接收的包被覆盖*/
	ret = setsockopt(rawsock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	if( ret < 0 ){
		perror("setsockopt SO_RCVBUF error");
		return -1 ;
	}

	/*自己填充IP头 发送的时候*/
	int set = 1 ;
	protocol = getprotobyname("ip");
	if (protocol == NULL){
		perror("getprotobyname()");
		return -1;
	}else{
		printf("getprotobyname %s %d\n", protocol->p_name, protocol->p_proto);
	}
	ret = setsockopt(rawsock, protocol->p_proto/*IPPROTO_IP*/, IP_HDRINCL, &set, sizeof(set));
	if( ret < 0 ){
		perror("setsockopt IP_HDRINCL error");
		return -1 ;
	}
	/*获得指定网卡的IP地址*/
	bzero(&src, sizeof(src));
	struct ifreq ifr ;
	int name_len = strlen (argv[1]) > IFNAMSIZ-1 ? IFNAMSIZ-1  : strlen (argv[1]);
	strncpy(ifr.ifr_name, argv[1] , name_len ); // IFNAMSIZ
	ifr.ifr_name[ name_len ] = '\0';
	if(ioctl(rawsock , SIOCGIFADDR, &ifr ) < 0 ){   /* SIOCGIFADDR Socket configuration controls. */
		perror("get %s ip address ERROR");
		return -1 ;
	}else{
		// error: conversion to non-scalar type requested
		//struct sockaddr_in inaddr =  (struct sockaddr_in)ifr.ifr_addr ;
		memcpy(&src , &ifr.ifr_addr , sizeof(struct sockaddr_in));
		printf("local network interface %s , ip_addr %s\n" ,  ifr.ifr_name , inet_ntoa(src.sin_addr) );

	}
	/*获取目的地址的IP地址*/
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	inaddr = inet_addr(argv[2]); // 输入的目的地址为字符串IP地址
	if(inaddr == INADDR_NONE){
		host = gethostbyname(argv[2]); // 输入的是DNS地址
		if(host == NULL){
			perror("gethostbyname");
			return -1;
		}

		/*host->h_addr_list数组 保存所有这个域名的ip地址
		 * 		h_addr         指向第一个ip地址
		 *  将第一个地址复制到dest中*/
		memcpy((char *)&dest.sin_addr, host->h_addr, host->h_length);

		printf("gethostbyname %s (%d.%d.%d.%d) ", argv[2],
					(dest.sin_addr.s_addr&0x000000FF)>>0,
					(dest.sin_addr.s_addr&0x0000FF00)>>8,
					(dest.sin_addr.s_addr&0x00FF0000)>>16,
					(dest.sin_addr.s_addr&0xFF000000)>>24 );
	}else{		// 为IP地址字符串
		memcpy((char*)&dest.sin_addr, &inaddr, sizeof(inaddr));
	}
	/*打印提示 ICMP不需要端口 只要设置IP地址即可 */
	inaddr = dest.sin_addr.s_addr;
	printf("PING %s (%ld.%ld.%ld.%ld) 56(84) bytes of data.\n", 
		dest_str, 
		(inaddr&0x000000FF)>>0,
		(inaddr&0x0000FF00)>>8,
		(inaddr&0x00FF0000)>>16,
		(inaddr&0xFF000000)>>24);// 大端 最低字节是最高位 192 in 192.168.1.100
	/*截取信号SIGINT，将icmp_sigint挂接上*/
	signal(SIGINT, icmp_sigint);
	alive = 1;						/*初始化为可运行*/
	pthread_t send_id, recv_id;		/*建立两个线程，用于发送和接收*/
	int err = 0;
	// 即使不发ICMP包的话，也会收到其他进程或者其他主机 对本进程主机发送的任何ICMP包
	err = pthread_create(&send_id, NULL, icmp_send, NULL);		/*发送*/
	if(err < 0)
	{
		return -1;
	}
	err = pthread_create(&recv_id, NULL, icmp_recv, NULL);		/*接收*/
	if(err < 0)
	{
		return -1;
	}
	
	/*等待线程结束*/
	pthread_join(send_id, NULL);
	pthread_join(recv_id, NULL);
	/*清理并打印统计结果*/
	close(rawsock);
	icmp_statistics();
	return 0;	
}

/*CRC16校验和计算icmp_cksum
参数：
	data:数据
	len:数据长度
返回值：
	计算结果，short类型
*/
static unsigned short icmp_cksum(unsigned char *data,  int len)
{
	int sum=0;							/*计算结果 32bit -> 64bit */
	int odd = len & 0x01;					/*是否为奇数*/

	// step 1. 按双字节累计 short 16bit
	/*将数据按照2字节为单位累加起来*/
	while( len & 0xfffe)  {
		sum += *(unsigned short*)data;
		data += 2;
		len -=2;
	}
	/*判断是否为奇数个数据，若ICMP报头为奇数个字节，会剩下最后一字节*/
	if( odd) {
		unsigned short tmp = ((*data)<<8)&0xff00;
		sum += tmp;
	}

	// step 2. 把高16bit和底16bit相加
	sum = (sum >>16) + (sum & 0xffff);	/*高低位相加*/

	// step 3. 把溢出到高16bit的再跟底16bit相加
	sum += (sum >>16) ;					/*将溢出位加入*/
	
	// ?? 这样就不会再有溢出??
	// ?? 为啥要取反 ??
	// ?? ICMP的校验和两个字节 不用管字节序号 ??
	return ~sum; 							/*返回取反值*/
}

/*设置ICMP报头*/
static void icmp_pack(struct icmp *icmph, int seq, struct timeval *tv, int length )
{
	unsigned char i = 0;
	/*设置报头*/
	icmph->icmp_type = ICMP_ECHO;	/*ICMP回显请求*/
	icmph->icmp_code = 0;			/*code值为0*/
	icmph->icmp_cksum = 0;	  /*先将cksum值填写0，便于之后的cksum计算*/
	icmph->icmp_seq = seq;			/*本报的序列号*/
	icmph->icmp_id = pid &0xffff;	/*填写PID*/
	for(i = 0; i< length; i++) // 实际这里拷贝多了  后面计算cksum 也只是算了64 这里只有64-4=60个字节 发送出去了
		icmph->icmp_data[i] = i;
									/*计算校验和*/
	icmph->icmp_cksum = icmp_cksum((unsigned char*)icmph, length);
}

/*设置IP报头*/
static void ip_pack(struct ip * ipheader , struct sockaddr_in src , struct sockaddr_in dest ,unsigned char* data_load , int data_len )
{
	// 要通过wireshark查看我们发送出去的IP数据报
	unsigned char i = 0;
	ipheader->ip_v = 4 ;
	ipheader->ip_hl = 20 /4 ;
	ipheader->ip_tos = 0 ;
	ipheader->ip_len = htons( 20 + data_len );
	static unsigned short ip_id = 0 ;
	ipheader->ip_id = htons(ip_id) ; // ??? 对方返回来的ICMP ECHO应答的IP数据报这个ID跟我们发出去的不一样
	ip_id ++ ;
	unsigned short off = IP_DF | (0 & IP_OFFMASK ) ; // do not Fragmant
	ipheader->ip_off = htons (off);
	ipheader->ip_ttl = 64 ;
	ipheader->ip_p = IPPROTO_ICMP;
	ipheader->ip_sum = 0 ;
	ipheader->ip_src = src.sin_addr ;
	ipheader->ip_dst = dest.sin_addr ;

	ipheader->ip_sum = icmp_cksum((unsigned char*)ipheader, 20 );
	memcpy((unsigned char*)ipheader + 20 , data_load , data_len );
}

/*解压接收到的包，并打印信息  buf 指向包含IP包头的IP数据报 */
static int icmp_unpack(char *buf,int len)
{
	int iphdrlen;
	struct ip *ip = NULL;
	struct icmp *icmp = NULL;
	int rtt;
	
	ip=(struct ip *)buf; 					/*IP头部*/

	printf(	"\n\nip header: version %d header len %d  type of service 0x%02x total len %d\n"
			"id %d RF %d DF %d MF %d fragment %d\n"
			"ttl %d proto %d ip_sum %d \n",
				ip->ip_v , ip->ip_hl * 4 , ip->ip_tos, ntohs(ip->ip_len),
				ntohs(ip->ip_id),IP_RF&ntohs(ip->ip_off)?1:0 ,IP_DF&ntohs(ip->ip_off)?1:0,IP_MF&ntohs(ip->ip_off)?1:0, IP_OFFMASK & ntohs(ip->ip_off) ,
				ip->ip_ttl  , ip->ip_p ,ip->ip_sum
				 );
	printf("src %s " , inet_ntoa(ip->ip_src) );
	printf("dst %s \n" , inet_ntoa(ip->ip_dst) );
	//	printf("src %s dst %s \n" ,
	//				inet_ntoa(ip->ip_src) ,
	//				inet_ntoa(ip->ip_dst) ); // 两个%s都是一样的
	//	char * buffer1 = inet_ntoa(ip->ip_src) ;
	//	char * buffer2 = inet_ntoa(ip->ip_dst) ;
	//	printf("%p %p \n",buffer1 , buffer2 );
	//	printf("%s %s \n",buffer1 , buffer2 );
	// 0x7f007a3636d8 0x7f007a3636d8  指向一样的内存空间
	// 192.168.42.182 192.168.42.182
	// inet_ntoa 会重复利用malloc出来的空间返回给用户 所以这里的打印会一样

	//printf("src 0x%08x dst 0x%08x\n" ,
	//		 ip->ip_src.s_addr, ip->ip_dst.s_addr);


	ip->ip_sum = 0 ;
	unsigned short new_sum = icmp_cksum((unsigned char *)ip , ip->ip_hl * 4  );
	printf("my ip chksum  = %d\n" , new_sum );

	iphdrlen=ip->ip_hl*4;					/*IP头部长度  不是直接+20 因为可能有IP选项*/
	icmp=(struct icmp *)(buf+iphdrlen);		/*ICMP段的地址*/
	len-=iphdrlen;

	if( len<8) 		/*判断长度是否为ICMP包*/
	{
		printf("ICMP packets\'s length is less than 8, dump data:\n");
		int i = 0  ;
		for(; i < len ; i++ ){
			printf("[%d] 0x%0x\n" , i , buf[iphdrlen + i]);
		}
		return -1;
	}
	printf(	"icmp header: type %d code %d "
			" pid %d , seq %d \n" ,
				icmp->icmp_type, icmp->icmp_code ,
				ntohs(icmp->icmp_id) , ntohs(icmp->icmp_seq) );
	// busybox/toolbox提供的ping命令 这两个有转换成 网络序(大端)

	/*
	 * 	这里会接收到本地的所有的ICMP协议的包 不管是自己发送出去还是接收回来的ICMP

		ip header: version 4 header len 20  type of service 0x00 total len 84
		id 19411 RF 0 DF 0 MF 0 fragment 64
		ttl 64 proto 1 ip_sum 54256
		src 127.0.0.1 dst 127.0.0.1
		my ip chksum  = 54256
		icmp header: type 8 code 0  pid 0 , seq 0  <= 这个是自己发送出去的ICMP ECHO请求

		(只有本地回环才会 再接收到自己发送出去的 ping本地的IP地址是不会再接受到自己的ICMP请求)


		ip header: version 4 header len 20  type of service 0x00 total len 84
		id 19412 RF 0 DF 0 MF 0 fragment 0
		ttl 64 proto 1 ip_sum 54064
		src 127.0.0.1 dst 127.0.0.1
		my ip chksum  = 54064
		icmp header: type 0 code 0  pid 0 , seq 0  <= 这个是自己发送出去的ICMP ECHO应答
		64 byte from 127.0.0.1: icmp_seq=0 ttl=64 rtt=0 ms


		192.168.42.182 是本机
		192.168.42.129 是远端

		ip header: version 4 header len 20  type of service 0x00 total len 84
		id 56309 RF 0 DF 0 MF 0 fragment 0
		ttl 64 proto 1 ip_sum 11208
		src 192.168.42.129 dst 192.168.42.182
		my ip chksum  = 11208
		icmp header: type 0 code 0  pid 15633 , seq 1792 <= 非本地回环 接收不到自己或者其他程序 发送出去的ICMP请求
		64 byte from 192.168.42.129: icmp_seq=7 ttl=64 rtt=1 ms

		ip header: version 4 header len 20  type of service 0x00 total len 84
		id 40045 RF 0 DF 0 MF 0 fragment 0
		ttl 64 proto 1 ip_sum 46087
		src 192.168.42.129 dst 192.168.42.182
		my ip chksum  = 46087
		icmp header: type 0 code 0  pid 4379 , seq 4  <= 其他程序发送出去的ICMP ECHO应答 但是没有捉到其他程序的请求 只有本地回环才会


		接收到其他机器发过来的ICMP请求  也就是其他主机 ping 本主机的某个网卡IP地址

		ip header: version 4 header len 20  type of service 0x00 total len 84
		id 38129 RF 0 DF 1 MF 0 fragment 0
		ttl 64 proto 1 ip_sum 12239
		src 192.168.42.129 dst 192.168.42.182  <= 其他主机 ping 本主机的某个网卡IP地址
		my ip chksum  = 12239
		icmp header: type 8 code 0  pid 407 , seq 1
	 */

	/*ICMP类型为ICMP_ECHOREPLY并且为本进程的PID*/
	if( (icmp->icmp_type==ICMP_ECHOREPLY) && (icmp->icmp_id== pid) )	
	{
		struct timeval tv_internel,tv_recv,tv_send;
		/*在发送表格中查找已经发送的包，按照seq*/
		pingm_pakcet* packet = icmp_findpacket(icmp->icmp_seq);
		if(packet == NULL)
			return -1;
		packet->flag = 0;	/*取消标志*/
		tv_send = packet->tv_begin;			/*获取本包的发送时间*/
		gettimeofday(&tv_recv, NULL);		/*读取此时间，计算时间差*/
		tv_internel = icmp_tvsub(tv_recv,tv_send);
		rtt = tv_internel.tv_sec*1000+tv_internel.tv_usec/1000; 
		/*打印结果，包含
		*  ICMP段长度
		*  源IP地址
		*  包的序列号
		*  TTL
		*  时间差
		*/
		printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%d ms\n",
			len,
			inet_ntoa(ip->ip_src),
			icmp->icmp_seq,
			ip->ip_ttl,
			rtt);
		
		packet_recv ++;						/*接收包数量加1*/
	}
	else
	{
		return -1;
	}
	return 0;
}

/*计算时间差time_sub
参数：
	end，接收到的时间
	begin，开始发送的时间
返回值：
	使用的时间
*/
static struct timeval icmp_tvsub(struct timeval end,struct timeval begin)
{
	struct timeval tv;
	/*计算差值*/
	tv.tv_sec = end.tv_sec - begin.tv_sec;
	tv.tv_usec = end.tv_usec - begin.tv_usec;
	/*如果接收时间的usec值小于发送时的usec值，从usec域借位*/
	if(tv.tv_usec < 0)
	{
		tv.tv_sec --;
		tv.tv_usec += 1000000; 
	}
	
	return tv;
}

/*发送ICMP回显请求包*/
static void* icmp_send(void *argv)
{
	/*保存程序开始发送数据的时间*/
	gettimeofday(&tv_begin, NULL);

	#define BUFFERSIZE 72					/*发送缓冲区大小*/
	char send_buff[BUFFERSIZE]; memset(send_buff , 0 , BUFFERSIZE );
	#define IP_TOTAL 64 + 20
	char ip_buff[IP_TOTAL]; memset(ip_buff , 0 , IP_TOTAL );

	while(alive)
	{
		int size = 0;
		struct timeval tv;
		gettimeofday(&tv, NULL);			/*当前包的发送时间*/
		/*在发送包状态数组中找一个空闲位置*/
		pingm_pakcet *packet = icmp_findpacket(-1);
		if(packet)
		{
			packet->seq = packet_send;		/*设置seq*/
			packet->flag = 1;				/*已经使用*/
			gettimeofday( &packet->tv_begin, NULL);	/*发送时间*/
		}
		
		/*打包数据*/
		icmp_pack((struct icmp *)send_buff, packet_send, &tv, 64 );// 64是ICMP总长度 不包含IP头部
		ip_pack((struct ip*)ip_buff , src , dest , send_buff , 64 );

//		size = sendto (rawsock,  send_buff, 64,  0,		/*发送给目的地址*/
//			(struct sockaddr *)&dest, sizeof(dest) );
		//size = send(rawsock , ip_buff , 64+20 , 0 ); // Destination address required 自己打包IP头部
		sendto (rawsock,  ip_buff, 64+20,  0,
					(struct sockaddr *)&dest, sizeof(dest) );
		if(size <0)
		{
			perror("sendto error");
			sleep(10);
			continue;
		}

		packet_send++;					/*计数增加*/
		/*每隔1s，发送一个ICMP回显请求包*/
		sleep(1);
	}
}

/*接收ping目的主机的回复*/
static void *icmp_recv(void *argv)
{
	/*轮询等待时间*/
	struct timeval tv;
	tv.tv_usec = 200;
	tv.tv_sec = 0;
	fd_set  readfd;
	struct sockaddr_in from;
	unsigned int len_from = sizeof(from);


	/*当没有信号发出一直接收数据*/
	while(alive)
	{
		int ret = 0;
		FD_ZERO(&readfd);
		FD_SET(rawsock, &readfd);
		ret = select(rawsock+1,&readfd, NULL, NULL, &tv);
		switch(ret)
		{
			case -1:
				/*错误发生*/
				break;
			case 0:
				/*超时*/
				break;
			default:
				{
					/*接收数据*/
					//int size = recv(rawsock, recv_buff,sizeof(recv_buff),  0);
					int size = recvfrom( rawsock, recv_buff, sizeof(recv_buff),
													MSG_TRUNC, // 如果报文大于缓冲区 依旧返回报文实际大小
													(struct sockaddr*)&from,
													&len_from);
					if(errno == EINTR){
						perror("recvfrom error");
						continue;
					}else if(size > sizeof(recv_buff) ){		/*缓冲区太小了*/
						printf("recefrom buffer too small\n");
						continue;
						// 如果不设置MSG_TRUNC
					}else{
						/*解包，并设置相关变量*/
						ret = icmp_unpack(recv_buff, size);
						if(ret == -1){
							continue;
						}
					}
				}
				break;
		}
		
	}
}

/*查找一个合适的包位置
*当seq为-1时，表示查找空包
*其他值表示查找seq对应的包*/
static pingm_pakcet *icmp_findpacket(int seq)
{
	int i=0;
	pingm_pakcet *found = NULL;
	/*查找包的位置*/
	if(seq == -1)							/*查找空包的位置*/
	{
		for(i = 0;i<128;i++)
		{
			if(pingpacket[i].flag == 0)
			{
				found = &pingpacket[i];
				break;
			}
			
		}
	}
	else if(seq >= 0)						/*查找对应seq的包*/
	{
		for(i = 0;i<128;i++)
		{
			if(pingpacket[i].seq == seq)
			{
				found = &pingpacket[i];
				break;
			}
			
		}
	}
	return found;
}

/*打印全部ICMP发送接收统计结果*/
static void icmp_statistics(void)
{       
	long time = (tv_interval.tv_sec * 1000 )+ (tv_interval.tv_usec/1000);
	printf("--- %s ping statistics ---\n",dest_str);	/*目的IP地址*/
	printf("%d packets transmitted, %d received, %d packet loss, time %ldms\n",
		packet_send,									/*发送*/
		packet_recv,  									/*接收*/
		packet_send == 0 ? 0 : (packet_send-packet_recv)*100/packet_send, 	/*丢失百分比*/
		time); 											/*时间*/
}

/*终端信号处理函数SIGINT*/
static void icmp_sigint(int signo)
{
	
	alive = 0;							/*告诉接收和发送线程结束程序*/	
	gettimeofday(&tv_end, NULL);		/*读取程序结束时间*/	
	tv_interval = icmp_tvsub(tv_end, tv_begin);  /*计算一下总共所用时间*/
	
	return;
}
