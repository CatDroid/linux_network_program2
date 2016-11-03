

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>						//	in_addr结构
// #include <linux/in.h>
#include <netinet/if_ether.h>				// 	struct 	ethhdr 以太网帧头 ETH_P_ALL
#include <netinet/ip.h>						//	struct	iphdr
#include <netinet/udp.h>					//	struct	udphdr
#include <netinet/tcp.h>					//	struct 	tcphdr
#include <net/if_arp.h>						//  struct  arphdr
// #include <linux/if_arp.h>
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
					struct sockaddr_ll from ;
					memset(&from,0,sizeof(struct sockaddr_ll));
					int fromlen = sizeof(struct sockaddr_ll);
					// 	在调用recvfrom函数时返回的地址长度信息为18字节(整个sockaddr_ll应该是20个字节)
					// 	原因是在sockaddr_ll结构中的sll_addr[8]为8字节
					//	MAC地址只使用了其中的前6字节
					ret = recvfrom(pctx->socket ,ef,ETH_FRAME_LEN,MSG_TRUNC,(struct sockaddr*)&from, &fromlen);


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
								printf("ARP request. from %x %x %x %x %x %x "
											"family %d protocol %x pkt_type %d halen %d recelen %d\n",
											from.sll_addr[0],
											from.sll_addr[1],
											from.sll_addr[2],
											from.sll_addr[3],
											from.sll_addr[4],
											from.sll_addr[5],
											from.sll_family ,
											ntohs(from.sll_protocol),
											from.sll_pkttype,
											from.sll_halen,
											fromlen);
								// 	ARP request. from 36 a7 52 19 77 9b family 17 protocol 1544 pkt_type 0 halen 6 recelen 18
								//	family 17 == AF_PACKET
								//	pkt_type 0  == PACKET_HOST 目标是给我们的以太网帧率
								//  halen 6 recelen 18 以太网物理地址之用了6个字节 sockaddr_ll定义了8个字节
							}else if( op ==  ARPOP_REPLY ){
								printf("ARP reply. from %x %x %x %x %x %x "
											"family %d protocol %x pkt_type %d halen %d recelen %d\n",
											from.sll_addr[0],
											from.sll_addr[1],
											from.sll_addr[2],
											from.sll_addr[3],
											from.sll_addr[4],
											from.sll_addr[5],
											from.sll_family ,
											ntohs(from.sll_protocol) , // be16
											from.sll_pkttype,
											from.sll_halen,
											fromlen);
							}else{
								printf("Address Resolution packet %d \n" , pArp->ar_op );
							}
						}break;
						case ETH_P_IP:
						{
							struct iphdr* pip = (struct iphdr*)(ef + sizeof(struct ethhdr));
							switch(pip->protocol){
							case IPPROTO_TCP:
								printf("Transmission Control Protocol.\n");
								break;
							case IPPROTO_UDP:
								printf("User Datagram Protocol.\n");
								break;
							case IPPROTO_ICMP:
								printf("Internet Control Message Protocol.\n");
								break;
							case IPPROTO_IGMP:
								printf("nternet Group Management Protocol.\n");
								break;
							default:
								printf("unknown Internet Protocol packet %d \n" , pip->protocol );
								break;
							}
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
							printf("from %02x:%02x:%02x:%02x:%02x:%02x to "
									"%02x:%02x:%02x:%02x:%02x:%02x\n",
									pethh->h_source[0],pethh->h_source[1],
									pethh->h_source[2],pethh->h_source[3],
									pethh->h_source[4],pethh->h_source[5],
									pethh->h_dest[0],pethh->h_dest[1],
									pethh->h_dest[2],pethh->h_dest[3],
									pethh->h_dest[4],pethh->h_dest[5]);
							printf("以太网帧 负载类型 不确定: 0x%x\n" ,  pethh->h_proto);
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

/* 长度为20字节
struct sockaddr_ll
{
	unsigned short sll_family; 		//  总是 AF_PACKET
	unsigned short sll_protocol; 	// 	物理层的协议
			 linux/if_ether.h 头文件中定义的按网络层排序的标准的以太桢协议类型
	int sll_ifindex; 				//  网卡对应的接口号 ('eth0'--> 0)
	unsigned short sll_hatype; 		//  报头类型
			 linux/if_arp.h 中定义的 ARP 硬件地址类型 物理层地址类型
	unsigned char sll_pkttype; 		//  分组类型
			目标地址是本地主机的分组用的 PACKET_HOST
			物理层广播分组用的 PACKET_BROADCAST
			发送到一个物理层多路广播地址的分组用的 PACKET_MULTICAST
			在混杂(promiscuous)模式下的设备驱动器发向其他主机的分组用的 PACKET_OTHERHOST
			本源于本地主机的分组被环回到分组套接口用的 PACKET_OUTGOING。
			这些类型只对接收到的分组有意义。
	unsigned char sll_halen; 		//  地址长度
	unsigned char sll_addr[8]; 		//  物理层地址
			sll_addr 和 sll_halen 包括物理层(例如 IEEE 802.3)地址和地址长度
			精确的解释依赖于设备
};

当在多个网络接口的主机上使用这个套接字时
若要指定接收或发送的接口时可以使用bind进行绑定
这与TCP套接字的操作一样，但其内涵并不相同。

绑定时将根据地址结构中的sll_protocal和sll_ifindex分别绑定收发的协议号和接口索引号
接口索引号sll_ifindex为0时表示使用有效的所有接口。


在linux环境中要从链路层（MAC）直接收发数据帧,可以通过libpcap与libnet两个动态库来分别完成收与发的工作

 * */
#define ARP_NET_INTERFACE "usb0"
int main(int argc, char*argv[])
{
	char ef[ETH_FRAME_LEN];  			// 以太帧缓冲区
	memset(ef,0,ETH_FRAME_LEN);


	struct ethhdr*p_ethhdr;				// 以太网头部指针

	char eth_dest[ETH_ALEN]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};/*目的以太网地址*/

	/**
	 * 数据链路层访问 协议栈不处理
	 * 套接字比较强大,创建这种套接字可以监听网卡上的所有数据帧
	 * 	能: 接收发往本地mac的数据帧
		能: 接收从本机发送出去的数据帧(第3个参数需要设置为ETH_P_ALL)
		能: 接收非发往本地mac的数据帧(网卡需要设置为promisc混杂模式)

		int socket(AF_PACKET, int type, int protocol)
		type:
			SOCK_RAW
				它是包含了MAC层头部信息的原始分组
				当然这种类型的套接字在发送的时候需要自己加上一个MAC头部（其类型定义在linux/if_ether.h中，ethhdr）
			SOCK_DGRAM
				它是已经进行了MAC层头部处理的,即收上的帧已经去掉了头部,而发送时也无须用户添加头部字段。

		Protocol:
			是指其送交的上层的协议号
			如IP为htons(ETH_P_IP)	0x0800
			当其为htons(ETH_P_ALL)  0x0003  收发所有的协议
	 */
	//
	struct context ctx ;
	ctx.socket = socket(AF_PACKET, SOCK_RAW, htons( ETH_P_ARP ) ) ;
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

	// 以太网帧头
	p_ethhdr = (struct ethhdr*)ef;
	memcpy(p_ethhdr->h_dest, eth_dest, ETH_ALEN);// 目的以太网地址
	memcpy(p_ethhdr->h_source,  (unsigned char*)ifr.ifr_hwaddr.sa_data , ETH_ALEN);
												 // 源以太网地址
	p_ethhdr->h_proto = htons(ETH_P_ARP);		 // 上层设置协议类型，ARP=0x0806

	// ARP帧内容
	struct arppacket*p_arp;
	p_arp = (struct arppacket*)(ef + ETH_HLEN);	// 定位ARP包地址
	p_arp->ar_hrd = htons(ARPHRD_ETHER);		// arp硬件类型
	p_arp->ar_pro = htons(ETH_P_IP);			// 协议类型
	p_arp->ar_hln = 6;							// 硬件地址长度
	p_arp->ar_pln = 4;							// IP地址长度
	p_arp->ar_op  = htons(ARPOP_REQUEST);		// ARP操作码  ARP请求 ARP应达 RARP请求 RARP应答

	memcpy(p_arp->ar_sha, ifr.ifr_hwaddr.sa_data, ETH_ALEN);// 复制源以太网地址
	p_arp->ar_sip = inet_addr("192.168.42.180");			// 源IP地址
	memcpy(p_arp->ar_tha, eth_dest, ETH_ALEN);				// 复制目的以太网地址 链路层广播 FF:FF:FF:FF:FF:FF
	p_arp->ar_tip = inet_addr("192.168.42.129");			// 目的IP地址

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

	//ret = write(ctx.socket, ef, ETH_FRAME_LEN);
	struct sockaddr_ll device ;
	memset(&device,0, sizeof(struct sockaddr_ll));
	// addr.sll_protocol = htons(ETH_P_ARP);
	device.sll_family = AF_PACKET ;
	if ((device.sll_ifindex = if_nametoindex (ARP_NET_INTERFACE)) == 0) {// 网卡对应的索引
		/**
		 * 	接口的sll_ifindex值可以通过ioctl获得
			strcpy(ifr.ifr_name,"eth0");
			ioctl(fd_packet,SIOCGIFINDEX,&ifr);
		 */
		perror ("if_nametoindex() failed to obtain interface index ");
		close(ctx.socket);
		return -1 ;
	}
	device.sll_hatype = ARPHRD_ETHER ; 		// 代表后面的是 链路层是以太网的地址
	memcpy (device.sll_addr, eth_dest, 6);	// 链路层地址
	device.sll_halen = htons (6); 			// 链路层地址长度 hardware address length

	int i = 0;
	ret = 0 ;
	for(i=0;i<5;i++){

		// 发送的data，长度可以任意，但是抓包时看到最小数据长度为46
		// 这是以太网协议规定以太网帧数据域部分最小为46字节，不足的自动补零处理
		sendto(ctx.socket , ef  ,ETH_HLEN + sizeof(struct arppacket) ,0, (const struct sockaddr *)&device, sizeof(struct sockaddr_ll));
		printf("send one arp! %zd ret = %d %d %s \n", sizeof(struct arppacket) ,ret ,errno ,strerror(errno));
		sleep(10);
	}

	write(pipefd[1],"",1);
	pthread_join(ctx.thread,NULL);
	close(ctx.socket);
	return 0;
}
