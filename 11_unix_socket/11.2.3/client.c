#define IP_FOUND "IP_FOUND"       			/*IP发现命令*/
#define IP_FOUND_ACK "IP_FOUND_ACK"		/*IP发现应答命令*/
#define IFNAME "eth0"
void IPFound(void*arg)
{
	#define BUFFER_LEN 32
	int ret = -1;
	SOCKET sock = -1;
	int so_broadcast = 1;
	struct ifreq ifr;  			
	struct sockaddr_in broadcast_addr;		/*本地地址*/
	struct sockaddr_in from_addr;			/*服务器端地址*/
	int from_len;
	int count = -1;
	fd_set readfd;
	char buff[BUFFER_LEN];
	struct timeval timeout;	
	timeout.tv_sec = 2;					/*超时时间2s*/
	timeout.tv_usec = 0;

	sock = socket(AF_INET, SOCK_DGRAM, 0);/*建立数据报套接字*/
	if( sock < 0 )
	{
		DBGPRINT("HandleIPFound: socket init error\n");
		return;
	}
	/*将需要使用的网络接口字符串名字复制到结构中*/
	strcpy(ifr.ifr_name,IFNAME,strlen(IFNAME));
	/*发送命令，获取网络接口的广播地址*/
	if(ioctl(sock,SIOCGIFBRDADDR,&ifr) == -1)
		perror("ioctl error"),exit(1);
	/*将获得的广播地址复制给变量broadcast_addr*/
	memcpy(&broadcast_addr, &ifr.ifr_broadaddr, sizeof(struct sockaddr_in ));
	broadcast_addr.sin_port = htons(MCAST_PORT);/*设置广播端口*/

	/*设置套接字文件描述符sock为可以进行广播操作*/
	ret = setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&so_broadcast,sizeof so_broadcast);

	/*主处理过程*/
	int times = 10;
	int i = 0;
	for(i=0;i<times;i++)
	{
		/*广播发送服务器地址请求*/
		ret = sendto(sock,IP_FOUND,strlen(IP_FOUND),0,(struct sockaddr*)&broadcast_addr,sizeof(broadcast_addr));
		if(ret == -1){
			continue;	
		}
		/*文件描述符集合清零*/
		FD_ZERO(&readfd);
		/*将套接字文件描述符加入读集合*/
		FD_SET(sock, &readfd);
		/*select侦听是否有数据到来*/
		ret = selectsocket(sock+1, &readfd, NULL, NULL, &timeout);
		switch(ret)
		{
			case -1:
				/*发生错误*/
				break;
			case 0:
				/*超时*/
				//超时所要执行的代码
				break;
			default:
  				/*有数据到来*/
				if( FD_ISSET( sock, &readfd ) )
				{
					/*接收数据*/
					count = recvfrom( sock, buff, BUFFER_LEN, 0,( struct sockaddr*) &from_addr, &from_len );
					DBGPRINT( "Recv msg is %s\n", buff );
					if(strstr(buff, IP_FOUND_ACK))/*判断是否吻合*/
					{
						printf("found server, IP is %s\n",inet_ntoa(from_addr.sin_addr));
					}
					break;/*成功获得服务器地址，退出*/
				}
		}
	}	
	return;
}