#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	int s;

	int err = -1;
	if(argc < 3){
		printf("错误的使用方式,格式为:\nioctl_arp inteface ip(ioctl_arp eth0 127.0.0.1)\n");
		return -1;
	}

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)	{
		printf("socket() 出错\n");
		return -1;
	}

	// 填充arpreq的成员arp_pa 为INET地址(家族和IP地址)
	struct arpreq arpreq;
	struct sockaddr_in *addr = (struct sockaddr_in*)&arpreq.arp_pa;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = inet_addr(argv[2]); // 查询某个IP地址
	if(addr->sin_addr.s_addr == INADDR_NONE){ 	// 判断inet_addr是否地址错误
		printf("IP 地址格式错误\n");
	}

	// 填充arpreq的成员arp_dev 网络接口

	strcpy(arpreq.arp_dev, argv[1]);
	printf("dev %s \n" , arpreq.arp_dev);
	err = ioctl(s, SIOCGARP, &arpreq); // 不会发送ARP请求
	if(err < 0){
		// 如果arp -a 看不到的 IP和MAC对应
		// 这里就会返回错误 ENXIO 6 No such device or address
		printf("IOCTL 错误 %d %s\n",errno,strerror(errno));
		return -1;
	}else{
		unsigned char *hw;
		hw = (unsigned char*)&arpreq.arp_ha.sa_data;
		printf("%s:",argv[1]);
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
				hw[0],hw[1],hw[2],hw[3],hw[4],hw[5]);
	}
	close(s);
	return 0;
}
