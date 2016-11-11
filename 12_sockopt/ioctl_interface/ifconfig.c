#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>



#define HW_NET_INTEFACE "usb0"


// 相当于 ifconfig
// ifconfig usb0 192.168.2.78
// ifconfig 设备名字 修改成的IP地址
int main(int argc, char *argv[])
{
	int s;									/*套接字描述符*/
	int err = -1;							/*错误值*/
	/* 可以用AF_UNIX 或者 AF_INET获取网卡信息
	 *
	 * 如果套接字类型是 AF_UNIX 可以获取网卡部分信息 除了IP地址
	 * 			错误 ENOTTY error 25 Inappropriate ioctl for device    Not a typewriter
	 *
	 * */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	//s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0)	{
		printf("socket() 出错\n");
		return -1;
	}

	{
		struct ifreq ifr;
		ifr.ifr_ifindex = 2;
		err = ioctl(s, SIOCGIFNAME, &ifr);
		if(err){
			printf("获取第 %d 块网络接口的名称 Error %d %s\n",ifr.ifr_ifindex , errno ,strerror(errno));
		}else{
			printf("第 %d 块网络接口名称是 is:%s\n",ifr.ifr_ifindex,ifr.ifr_name);
		}
	}

	const char* interface_name ;
	if(argc > 1 ){
		interface_name = argv[1] ;
	}else{
		interface_name = HW_NET_INTEFACE ;
	}
	{
		struct ifreq ifr;
		memcpy(ifr.ifr_name, interface_name ,strlen(interface_name));

		err = ioctl(s, SIOCGIFINDEX, &ifr);
		if(!err){
			printf("获取网卡序号:%d\n",ifr.ifr_ifindex);
		}

		err = ioctl(s, SIOCGIFFLAGS, &ifr);
		if(!err){
			printf("获取标记:%08x\n",ifr.ifr_flags);
		}
		if(ifr.ifr_flags & IFF_UP ) printf("UP ");
		if(ifr.ifr_flags & IFF_BROADCAST ) 	printf("BROADCAST ");
		if(ifr.ifr_flags & IFF_MULTICAST ) 	printf("MULTICAST ");
		if(ifr.ifr_flags & IFF_RUNNING    ) printf("RUNNING ");
		if(ifr.ifr_flags & IFF_LOOPBACK ) 	printf("LOOPBACK ");
		if(ifr.ifr_flags & IFF_NOARP ) 		printf("NOARP ");
		if(ifr.ifr_flags & IFF_PROMISC ) 	printf("PROMISC ");
		if(ifr.ifr_flags & IFF_ALLMULTI ) 	printf("ALLMULTI ");
		if(ifr.ifr_flags & IFF_ALLMULTI ) 	printf("ALLMULTI ");
		printf("\n");
		/* 当ifconfig eth0 up时，使用set_flag置位IFF_UP和IFF_RUNNING
		 *
		 * 	设备标志
			IFF_UP			接口正在运行.
			IFF_BROADCAST	有效的广播地址集.
			IFF_DEBUG		内部调试标志.
			IFF_LOOPBACK	这是自环接口.
			IFF_POINTOPOINT	这是点到点的链路接口.
			IFF_RUNNING		资源已分配.
			IFF_NOARP		无arp协议, 没有设置第二层目的地址.
			IFF_PROMISC		接口为杂凑(promiscuous)模式.
			IFF_NOTRAILERS	避免使用trailer .
			IFF_ALLMULTI	接收所有组播(multicast)报文.
			IFF_MASTER		主负载平衡群(bundle).
			IFF_SLAVE		从负载平衡群(bundle).
			IFF_MULTICAST	支持组播(multicast).
			IFF_PORTSEL		可以通过ifmap选择介质(media)类型.
			IFF_AUTOMEDIA	自动选择介质.
			IFF_DYNAMIC		接口关闭时丢弃地址.

			设置 活动标志字 是 特权操作, 但是 任何进程 都可以 读取 标志字.
		 */

		err = ioctl(s, SIOCGIFMETRIC, &ifr);
		if(!err){
			printf("获取METRIC:%d\n",ifr.ifr_metric);
		}

		err = ioctl(s, SIOCGIFMTU, &ifr);
		if(!err){
			printf("获取MTU:%d\n",ifr.ifr_mtu);
		}	

		err = ioctl(s, SIOCGIFHWADDR, &ifr);
		if(!err){
			unsigned char *hw = ifr.ifr_hwaddr.sa_data; // 需要用unsigned 不然 %x 会先转成有符号整数
			printf("获取MAC地址:%02x:%02x:%02x:%02x:%02x:%02x\n",hw[0],hw[1],hw[2],hw[3],hw[4],hw[5]);
		}

		err = ioctl(s, SIOCGIFMAP, &ifr);
		if(!err){ // 目前测试 eth0 usb0都是全部0 root权限运行一样
			printf("网卡映射参数,mem_start:%ld,mem_end:%ld, base_addr:%d, irq:%d, dma:%d,port:%d\n",
				ifr.ifr_map.mem_start, 		/*开始地址*/
				ifr.ifr_map.mem_end,		/*结束地址*/
				ifr.ifr_map.base_addr,		/*基地址*/
				ifr.ifr_map.irq ,			/*中断*/
				ifr.ifr_map.dma ,			/*直接访问内存*/
				ifr.ifr_map.port );			/*端口*/
		}

		err = ioctl(s, SIOCGIFTXQLEN, &ifr);
		if(!err){
			printf("发送队列长度:%d\n",ifr.ifr_qlen);
		}			
	}

	/*
	 * 获得网络接口IP地址
	 * 如果网卡没有被分配IP地址等 会遇到错误 ADDR NOT AVAIL
	 * EADDRNOTAVAIL  99 Cannot assign requested address
	 */
	{
		struct ifreq ifr;
		struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
		char ip[16];
		memset(ip, 0, 16);
		memcpy(ifr.ifr_name, interface_name ,strlen(interface_name));
		

		err = ioctl(s, SIOCGIFADDR, &ifr);
		if(!err){
			inet_ntop(AF_INET, &sin->sin_addr.s_addr, ip, 16 );
			printf("本地IP地址:%s\n",ip);
		}else{
			printf("本地IP地址 error %d %s \n", errno ,strerror(errno));
		}
		
		err = ioctl(s, SIOCGIFBRDADDR, &ifr);
		if(!err){
			inet_ntop(AF_INET, &sin->sin_addr.s_addr, ip, 16 );
			printf("广播IP地址:%s\n",ip);
		}else{
			printf("广播IP地址 error %d %s \n", errno ,strerror(errno));
		}
		
		err = ioctl(s, SIOCGIFDSTADDR, &ifr);
		if(!err){
			inet_ntop(AF_INET, &sin->sin_addr.s_addr, ip, 16 );
			printf("目的IP地址:%s\n",ip);
		}else{
			printf("目的IP地址 error %d %s \n", errno ,strerror(errno));
		}
		
		err = ioctl(s, SIOCGIFNETMASK, &ifr);
		if(!err){
			inet_ntop(AF_INET, &sin->sin_addr.s_addr, ip, 16 );
			printf("子网掩码 :%s\n",ip);
		}else{
			printf("子网掩码 error %d %s \n", errno ,strerror(errno));
		}
	}

#if 1
	/*
	 * 测试更改IP地址 特权操作
	 *
	 * root 否者权限 error 1 Operation not permitted
	 *
	 * 将字符串转换为网络字节序的整型  inet_pton
	 */
	{
#define MODIFY_TO_IP "192.168.42.175"

		const char* modify_to ;
		if( argc > 2 ){
			modify_to = argv[2];
		}else{
			modify_to = MODIFY_TO_IP;
		}

		struct ifreq ifr;
		/*方便操作设置指向sockaddr_in的指针*/
		struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;


		printf("Set %s IP to %s\n" , interface_name ,  modify_to );
		memset(&ifr, 0, sizeof(ifr));
		memcpy(ifr.ifr_name, interface_name ,strlen(interface_name));
		inet_pton(AF_INET, modify_to , &sin->sin_addr.s_addr);
		//sin->sin_addr.s_addr = inet_addr("192.168.1.175");
		sin->sin_family = AF_INET;			/*协议族*/
		err = ioctl(s, SIOCSIFADDR, &ifr);	/*发送设置本机IP地址请求命令*/
		if(err){
			printf("SIOCSIFADDR error %d %s \n" ,errno ,strerror(errno));
		}else{
			printf("check IP --");
			memset(&ifr, 0, sizeof(ifr));
			memcpy(ifr.ifr_name,interface_name ,strlen(interface_name));
			ioctl(s, SIOCGIFADDR, &ifr);
			char ip[16];
			inet_ntop(AF_INET, &sin->sin_addr.s_addr, ip, 16);
			printf("%s\n",ip);
		}		
	}
#endif
	close(s);
	return 0;
}
