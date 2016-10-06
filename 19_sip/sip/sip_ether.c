#include "sip.h"

static struct net_device ifdevice;
struct net_device *get_netif()
{
	return &ifdevice;
}
void DISPLAY_MAC(struct sip_ethhdr* eth)
{
	printf("From:%02x-%02x-%02x-%02x-%02x-%02x          ",
		eth->h_source[0],		eth->h_source[1],		eth->h_source[2],
		eth->h_source[3],		eth->h_source[4],		eth->h_source[5]);
	printf("to:%02x-%02x-%02x-%02x-%02x-%02x\n",
		eth->h_dest[0],		eth->h_dest[1],		eth->h_dest[2],
		eth->h_dest[3],		eth->h_dest[4],		eth->h_dest[5]);

}
static __u8 input(struct skbuff *pskb, struct net_device *dev)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>input\n");
	char ef[ETH_FRAME_LEN];  	/*以太帧缓冲区*/
	int n,i;			
	int retval = 0;

	/*读取以太网数据，n为返回的实际捕获的以太帧的帧长*/
	n = read(dev->s, ef, ETH_FRAME_LEN);   
	if(n < 0)	{
		DBGPRINT(DBG_LEVEL_ERROR,"Not datum\n");
		retval = -1;
		goto EXITinput;
	}else{
		DBGPRINT(DBG_LEVEL_NOTES,"%d bytes datum\n", n);
	};	

	struct skbuff *skb = skb_alloc(n);
	if(!skb){
		retval = -1;
		goto EXITinput;
	}
	
	memcpy(skb->head, ef, n);
	skb->tot_len =skb->len= n;
	skb->phy.ethh= (struct sip_ethhdr*)skb_put(skb, sizeof(struct sip_ethhdr));
	//DISPLAY_MAC(skb->phy.ethh);
	if(samemac(skb->phy.ethh->h_dest, dev->hwaddr) 
		|| samemac(skb->phy.ethh->h_dest, dev->hwbroadcast))
	{
		DBGPRINT(DBG_LEVEL_NOTES,"local in parsing...\n");
		switch(htons(skb->phy.ethh->h_proto))
		{
			case ETH_P_IP:
				DBGPRINT(DBG_LEVEL_NOTES,"ETH_P_IP coming\n");
				
				skb->nh.iph = (struct sip_iphdr*)skb_put(skb, sizeof(struct sip_iphdr));
				arp_add_entry(skb->nh.iph->saddr, skb->phy.ethh->h_source, ARP_ESTABLISHED);
				
				ip_input(dev, skb);
				break;
			case ETH_P_ARP:
			{
				DBGPRINT(DBG_LEVEL_ERROR,"ETH_P_ARP coming\n");
				skb->nh.arph = (struct sip_arphdr*)skb_put(skb, sizeof(struct sip_arphdr));								
				if(*((__be32*)skb->nh.arph->ar_tip) == dev->ip_host.s_addr){
					
					arp_input(&skb, dev);
				}
				skb_free(skb);
			}
				break;
			default:
				DBGPRINT(DBG_LEVEL_ERROR,"ETHER:UNKNOWN\n");
				skb_free(skb);
				break;
		}
	}
	else
	{
		//DBGPRINT(DBG_LEVEL_ERROR,"NOT local, drop it\n");
		skb_free(skb);
	}

EXITinput:
	DBGPRINT(DBG_LEVEL_TRACE,"<==input\n");
	return 0;
}

static __u8 output(struct skbuff *skb, struct net_device *dev)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>output\n");
	int retval = 0;
	
	struct arpt_arp  *arp = NULL;
	int times = 0,found = 0;

	__be32 destip = skb->nh.iph->daddr;
	if((skb->nh.iph->daddr & dev->ip_netmask.s_addr ) != (dev->ip_host.s_addr & dev->ip_netmask.s_addr))
	{
		destip = dev->ip_gw.s_addr;
	}
	while((arp = 	arp_find_entry(destip)) == NULL && times < 5){
		arp_request(dev,destip);
		sleep(1);
		times ++;
	}

	if(!arp){
		retval = 1;
		goto EXIToutput;
	}	else	{
		struct sip_ethhdr *eh = skb->phy.ethh;
		memcpy(eh->h_dest, arp->ethaddr, ETH_ALEN);
		memcpy(eh->h_source, dev->hwaddr, ETH_ALEN);
		eh->h_proto = htons(ETH_P_IP);
		dev->linkoutput(skb,dev);
	}
EXIToutput:
	DBGPRINT(DBG_LEVEL_TRACE,"<==output\n");
	return retval;
}

static __u8 lowoutput(struct skbuff *skb, struct net_device *dev)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>lowoutput\n");

	int n = 0;
	int len = sizeof(struct sockaddr);
	struct skbuff *p =NULL;
	for(p=skb;p!= NULL;skb= p, p=p->next, skb_free(skb),skb=NULL)
	{
		n = sendto(dev->s, skb->head, skb->len,0, &dev->to, len);
		DBGPRINT(DBG_LEVEL_NOTES,"Send Number, n:%d\n",n);
	}

	DBGPRINT(DBG_LEVEL_TRACE,"<==lowoutput\n");

	return 0;
}

static void sip_init_ethnet(struct net_device *dev)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>sip_init_ethnet\n");

	memset(dev, 0, sizeof(struct net_device));

	dev->s = socket(AF_INET, SOCK_PACKET, htons(ETH_P_ALL));
	if(dev->s > 0)
	{
		DBGPRINT(DBG_LEVEL_NOTES,"create SOCK_PACKET fd success\n");
	}
	else
	{
		DBGPRINT(DBG_LEVEL_ERROR,"create SOCK_PACKET fd falure\n");
		exit(-1);
	}
	
	strcpy(dev->name, "eth1");
	memset(&dev->to, '\0', sizeof(struct sockaddr));
	dev->to.sa_family = AF_INET;
	strcpy(dev->to.sa_data, dev->name);  /* or whatever device */
	int r = bind(dev->s, &dev->to, sizeof(struct sockaddr));
	
	memset(dev->hwbroadcast, 0xFF, ETH_ALEN);
#if 0	
	dev->hwaddr[0] = 0x00;
	dev->hwaddr[1] = 0x12;
	dev->hwaddr[2] = 0x34;
	dev->hwaddr[3] = 0x56;
	dev->hwaddr[4] = 0x78;
	dev->hwaddr[5] = 0x90;
#else
//00:0C:29:73:9D:15
//00:0C:29:73:9D:1F
	dev->hwaddr[0] = 0x00;
	dev->hwaddr[1] = 0x0c;
	dev->hwaddr[2] = 0x29;
	dev->hwaddr[3] = 0x73;
	dev->hwaddr[4] = 0x9D;
	dev->hwaddr[5] = 0x1F;
#endif
	dev->hwaddr_len = ETH_ALEN;
#if 0
	dev->ip_host.s_addr = inet_addr("192.168.1.250");
	dev->ip_gw.s_addr = inet_addr("192.168.1.1");
	dev->ip_netmask.s_addr = inet_addr("255.255.255.0");
	dev->ip_broadcast.s_addr = inet_addr("192.168.1.255");
	
	dev->ip_host.s_addr = inet_addr("10.10.10.250");
	dev->ip_gw.s_addr = inet_addr("10.10.10..1");
	dev->ip_netmask.s_addr = inet_addr("255.255.255.0");
	dev->ip_broadcast.s_addr = inet_addr("10.10.10.255");
#else
	dev->ip_host.s_addr = inet_addr("172.16.12.250");
	dev->ip_gw.s_addr = inet_addr("172.16.12.1");
	dev->ip_netmask.s_addr = inet_addr("255.255.255.0");
	dev->ip_broadcast.s_addr = inet_addr("172.16.12.255");
#endif
	dev->input = input;
	dev->output = output;
	dev->linkoutput = lowoutput;
	dev->type = ETH_P_802_3;
	DBGPRINT(DBG_LEVEL_TRACE,"<==sip_init_ethnet\n");
}

struct net_device * sip_init(void)
{
	sip_init_ethnet(&ifdevice);

	return &ifdevice;
}

