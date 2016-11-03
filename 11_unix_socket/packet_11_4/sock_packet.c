

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>						//	in_addr结构
#include <netinet/if_ether.h>				// 	struct 	ethhdr 以太网帧头 ETH_P_ALL
#include <netinet/ip.h>						//	struct	iphdr
#include <netinet/udp.h>					//	struct	udphdr
#include <netinet/tcp.h>					//	struct 	tcphdr
#include <net/if_arp.h>						//  struct  arphdr
#include <linux/sockios.h>					//  SIOCGIFADDR 网卡的IP地址  SIOCGIFBRDADDR网卡的广播地址
#include <net/if.h>							//  IFNAMSIZ   struct ifreq
#include <linux/if_packet.h>				//	struct sockaddr_pkt

//#pragma pack (1)
struct arppacket // 有区别与 if_arp.h 的 struct arphdr 针对以太网 扩充了
{
	unsigned short	ar_hrd;				/*硬件类型*/
	unsigned short	ar_pro;				/*协议类型*/
	unsigned char	ar_hln;				/*硬件地址长度*/
	unsigned char	ar_pln;				/*协议地址长度*/
	unsigned short	ar_op;				/*ARP操作码*/
	unsigned char	ar_sha[ETH_ALEN];	/*发送方MAC地址*/
	in_addr_t	ar_sip;				/*发送方IP地址*/
	unsigned char	ar_tha[ETH_ALEN];	/*目的MAC地址*/
	in_addr_t	ar_tip;				/*目的IP地址*/
}__attribute__ ((packed)); // 必须取消字节对齐优化
//#pragma pack (0)

struct context
{
	int socket ;
	int piperd ;
	pthread_t thread ;
};

void* receive_thread(void* arg)
{
	struct context* pctx = (struct context*)arg ;
	int exitflag = 0 ;
	fd_set readfd ;
	int max_fd = -1 ;

	char ef[ETH_FRAME_LEN];

	int ret  = 0 ;
	while(!exitflag)
	{
		FD_ZERO(&readfd);
		FD_SET(pctx->socket, &readfd); if( pctx->socket > max_fd ) max_fd = pctx->socket;
		FD_SET(pctx->piperd, &readfd); if( pctx->piperd > max_fd ) max_fd = pctx->piperd ;
		ret = select(max_fd + 1, &readfd, NULL, NULL, NULL);
		switch(ret){
			case -1:
				printf("select error:%d,%s\n" , errno , strerror(errno) );
				break;
			case 0:
				printf("timeout\n");
				break;
			default:
				if( FD_ISSET(pctx->piperd, &readfd) ){
					printf("receive_thread exit !\n");
					exitflag = 1 ;
					break;
				}
				if( FD_ISSET( pctx->socket , &readfd ) ){
					struct sockaddr_pkt from ;
					memset(&from,0,sizeof(struct sockaddr_pkt));
					int fromlen = sizeof(struct sockaddr_pkt);
					ret = recvfrom(pctx->socket ,ef,ETH_FRAME_LEN,MSG_TRUNC,(struct sockaddr*)&from, &fromlen);
					//ret = read(pctx->socket , ef , ETH_FRAME_LEN);

					if( ret < 0){
						printf("read error! %d %s \n", errno , strerror(errno));
						break;
					}else if(ret > ETH_FRAME_LEN){
						printf("物理帧过大 已截断\n");
					}

					if( ret >= 14 ){
						struct ethhdr* pethh = (struct ethhdr*)ef ;

						switch( ntohs( pethh->h_proto)){
						case ETH_P_ARP:
						{
							struct arphdr * pArp = (struct arphdr*)(ef + ETH_HLEN) ;
							unsigned short int op = ntohs(pArp->ar_op) ;
							if(op  == ARPOP_REQUEST){
								printf("ARP request.\n");
							}else if( op ==  ARPOP_REPLY ){
								printf("ARP reply.\n");
							}else{
								printf("Address Resolution packet %d \n" , pArp->ar_op );
							}
						}break;
						case ETH_P_IP:
						{
//							struct iphdr* pip = (struct iphdr*)(ef + sizeof(struct ethhdr));
//							switch(pip->protocol){
//							case IPPROTO_TCP:
//								printf("Transmission Control Protocol.\n");
//								break;
//							case IPPROTO_UDP:
//								printf("User Datagram Protocol.\n");
//								break;
//							case IPPROTO_ICMP:
//								printf("Internet Control Message Protocol.\n");
//								break;
//							case IPPROTO_IGMP:
//								printf("nternet Group Management Protocol.\n");
//								break;
//							default:
//								printf("unknown Internet Protocol packet %d \n" , pip->protocol );
//								break;
//							}

						}break;
						case ETH_P_RARP:
							printf("Reverse Addr Res packet\n");
							break;
						case ETH_P_PPP_DISC:
							printf("PPPoE discovery messages\n");
							break;
						case ETH_P_PPP_SES:
							printf("PPPoE session messages\n");
							break;
						case 0x05DC:
							printf("802.3以太网帧 附加LLC和SNAP\n");
							break;
						default:
//							printf("from %02x:%02x:%02x:%02x:%02x:%02x to "
//									"%02x:%02x:%02x:%02x:%02x:%02x\n",
//									pethh->h_source[0],pethh->h_source[1],
//									pethh->h_source[2],pethh->h_source[3],
//									pethh->h_source[4],pethh->h_source[5],
//									pethh->h_dest[0],pethh->h_dest[1],
//									pethh->h_dest[2],pethh->h_dest[3],
//									pethh->h_dest[4],pethh->h_dest[5]);
//							printf("以太网帧 负载类型 不确定: 0x%x\n" ,  pethh->h_proto);
							break;
						}
					}else{
						printf("以太网帧太小");
					}
				}
		}
	}
	return NULL;
}

#define ARP_NET_INTERFACE "usb0"

int main(int argc, char*argv[])
{
	char ef[ETH_FRAME_LEN];  			/*以太帧缓冲区*/
	struct ethhdr*p_ethhdr;				/*以太网头部指针*/

	memset(ef,0,ETH_FRAME_LEN);

	char eth_dest[ETH_ALEN]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};/*目的以太网地址*/


	struct context ctx ;
	// ctx.socket = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL));
	ctx.socket = socket(AF_INET, SOCK_PACKET, htons( ETH_P_ARP ) ) ;// 数据链路层访问 协议栈不处理
	if( ctx.socket  < 0){
		printf("socket SOCK_PACKET error %d %s\n" , errno, strerror(errno) );
		return -1;
	}
	
	// 获得某个设备接口的MAC地址
	struct ifreq ifr;
	strncpy(ifr.ifr_name,ARP_NET_INTERFACE,strlen(ARP_NET_INTERFACE));
	if(ioctl(ctx.socket,SIOCGIFHWADDR,&ifr) == -1){
		perror("ioctl error"); // ioctl error: No such device 如果没有对应网络接口
		return -1 ;
	}
	if (ifr.ifr_hwaddr.sa_family!=ARPHRD_ETHER) {
		printf("different types of network interface\n");
	}else{
		const unsigned char* mac=(unsigned char*)ifr.ifr_hwaddr.sa_data;
		printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
			mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	}


	p_ethhdr = (struct ethhdr*)ef;
	memcpy(p_ethhdr->h_dest, eth_dest, ETH_ALEN); /*复制目的以太网地址*/
	memcpy(p_ethhdr->h_source,  (unsigned char*)ifr.ifr_hwaddr.sa_data , ETH_ALEN);/*复制源以太网地址*/
	p_ethhdr->h_proto = htons(ETH_P_ARP);/*设置协议类型，ARP=0x0806*/
	
	struct arppacket*p_arp;	
	p_arp = (struct arppacket*)(ef + ETH_HLEN);				/*定位ARP包地址*/
	p_arp->ar_hrd = htons(ARPHRD_ETHER);			/*arp硬件类型*/
	// ETHERTYPE_ARP与ETH_P_ARP的值都是0x0806，只是定义的文件不同
	// 前者定义在net/ethernet.h,后者定义在linux/if_ether.h
	p_arp->ar_pro = htons(ETH_P_IP);		/*协议类型*/
	p_arp->ar_op  = htons(ARPOP_REQUEST);
	p_arp->ar_hln = 6;					/*硬件地址长度*/
	p_arp->ar_pln = 4;					/*IP地址长度*/
	
	memcpy(p_arp->ar_sha, ifr.ifr_hwaddr.sa_data, ETH_ALEN);/*复制源以太网地址*/
	p_arp->ar_sip = inet_addr("192.168.42.180");/*源IP地址*/
	memcpy(p_arp->ar_tha, eth_dest, ETH_ALEN);/*复制目的以太网地址*/
	p_arp->ar_tip = inet_addr("192.168.42.129");/*目的IP地址*/

	int pipefd[2] = {0,0};
	pipe(pipefd);
	ctx.piperd = pipefd[0];


	int ret = pthread_create(&ctx.thread, 0 ,receive_thread , &ctx );
	if( ret < 0 ){
		printf("pthread_create error %d %s\n", errno , strerror(errno));
		close(ctx.socket);
		return -1;
	}

	int so_broadcast = 1;
	ret = setsockopt(ctx.socket,SOL_SOCKET,SO_BROADCAST,&so_broadcast,sizeof so_broadcast);
	if( ret < 0){
		printf("SOL_SOCKET SO_BROADCAST error %d %s\n", errno , strerror(errno));
	}

	/*发送ARP请求8次，间隔1s*/
	int i = 0;
	ret = 0 ;
	for(i=0;i<5;i++){

		//ret = write(ctx.socket, ef, ETH_FRAME_LEN);
		struct sockaddr_pkt to ;
		memset(&to,0, sizeof(struct sockaddr_pkt));
		strcpy(to.spkt_device,ARP_NET_INTERFACE);
		sendto(ctx.socket , ef  ,ETH_HLEN + sizeof(struct arppacket) ,0, (const struct sockaddr *)&to, sizeof(struct sockaddr_pkt));

		printf("send one arp! %zd ret = %d %d %s \n", sizeof(struct arppacket) ,ret ,errno ,strerror(errno));
		// 如果不设置 地址  出现错误 ret = -1 107 Transport endpoint is not connected
		sleep(10);
	}

	write(pipefd[1],"",1);
	pthread_join(ctx.thread,NULL);
	close(ctx.socket);
	return 0;
}
