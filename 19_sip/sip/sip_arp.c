#include "sip.h"


static struct arpt_arp arp_table[ARP_TABLE_SIZE];
void init_arp_entry()
{
	int i= 0;
	for(i = 0; i<ARP_TABLE_SIZE; i++)
	{
		arp_table[i].ctime = 0;
		memset(arp_table[i].ethaddr, 0, ETH_ALEN);
		arp_table[i].ipaddr = 0;
		arp_table[i].status = ARP_EMPTY;
	}
}
struct arpt_arp* arp_find_entry(__u32 ip)
{
	int i = -1;
	struct arpt_arp*found = NULL;
	for(i = 0; i<ARP_TABLE_SIZE; i++)
	{
		if(arp_table[i].ctime >  time(NULL) + ARP_LIVE_TIME)
			arp_table[i].status = ARP_EMPTY;
		else if(arp_table[i].ipaddr == ip 
			&& arp_table[i].status == ARP_ESTABLISHED)
		{
			found = &arp_table[i];
			break;
		}
	}

	return found;
}

struct arpt_arp  * update_arp_entry(__u32 ip,  __u8 *ethaddr)
{
	struct arpt_arp  *found = NULL;
	found = arp_find_entry(ip);
	if(found){
		memcpy(found->ethaddr, ethaddr, ETH_ALEN);
		found->status = ARP_ESTABLISHED;
		found->ctime = time(NULL);
	}

	return found;
}

void arp_add_entry(__u32 ip, __u8 *ethaddr, int status)
{
	int i = 0;
	struct arpt_arp  *found = NULL;
	found = update_arp_entry(ip, ethaddr);
	if(!found)
	{
		for(i = 0; i<ARP_TABLE_SIZE; i++)
		{
			if(arp_table[i].status == ARP_EMPTY)
			{
				found = &arp_table[i];
				break;
			}
		}
	}

	if(found){
		found->ipaddr = ip;
		memcpy(found->ethaddr, ethaddr, ETH_ALEN);
		found->status = status;
		found->ctime = time(NULL);
	}
}


/*
 *	Interface to link layer: send routine and receive handler.
 */

/*
 *	Create an arp packet. If (dest_hw == NULL), we create a broadcast
 *	message.
 */
struct skbuff *arp_create(struct net_device *dev, 		/*设备*/
							int type, 					/*ARP协议的类型*/
							__u32 src_ip,					/*源主机IP*/
							__u32 dest_ip,					/*目的主机IP*/
							__u8* src_hw,		/*源主机MAC*/							
							__u8* dest_hw, 		/*目的主机MAC*/							
							__u8* target_hw)		/*解析的主机MAC*/
{
	struct skbuff *skb;
	struct sip_arphdr *arph;
	DBGPRINT(DBG_LEVEL_TRACE,"==>arp_create\n");
	/*
	 *	Allocate a buffer
	 */	
	skb = skb_alloc(ETH_ZLEN);
	if (skb == NULL)
	{
		goto EXITarp_create;
	}

	skb->phy.raw = skb_put(skb,sizeof(struct sip_ethhdr));
	skb->nh.raw = skb_put(skb,sizeof(struct sip_arphdr));
	skb->dev = dev;
	if (src_hw == NULL)
		src_hw = dev->hwaddr;
	if (dest_hw == NULL)
		dest_hw = dev->hwbroadcast;

	skb->phy.ethh->h_proto = htons(ETH_P_ARP);
	memcpy(skb->phy.ethh->h_dest, dest_hw, ETH_ALEN);
	memcpy(skb->phy.ethh->h_source, src_hw, ETH_ALEN);

	skb->nh.arph->ar_op = htons(type);
	skb->nh.arph->ar_hrd = htons(ETH_P_802_3);
	skb->nh.arph->ar_pro =  htons(ETH_P_IP);
	skb->nh.arph->ar_hln = ETH_ALEN;
	skb->nh.arph->ar_pln = 4;

	memcpy(skb->nh.arph->ar_sha, src_hw, ETH_ALEN);
	memcpy(skb->nh.arph->ar_sip,  (__u8*)&src_ip, 4);
	memcpy(skb->nh.arph->ar_tip, (__u8*)&dest_ip, 4);
	if (target_hw != NULL)
		memcpy(skb->nh.arph->ar_tha, target_hw, dev->hwaddr_len);
	else
		memset(skb->nh.arph->ar_tha, 0, dev->hwaddr_len);
EXITarp_create:	
	DBGPRINT(DBG_LEVEL_TRACE,"<==arp_create\n");
	return skb;
	
}



void arp_send(struct net_device *dev, 		/*设备*/
				int type, 					/*ARP协议的类型*/
				__u32 src_ip,					/*源主机IP*/
				__u32 dest_ip,					/*目的主机IP*/
				__u8* src_hw,		/*源主机MAC*/							
				__u8* dest_hw, 		/*目的主机MAC*/							
				__u8* target_hw)		/*解析的主机MAC*/
{
	struct skbuff *skb;
	DBGPRINT(DBG_LEVEL_TRACE,"==>arp_send\n");
	skb = arp_create(dev,type,src_ip,dest_ip,src_hw,dest_hw,target_hw);
	if(skb){
		dev->linkoutput(skb, dev);
	}
	DBGPRINT(DBG_LEVEL_TRACE,"<==arp_send\n");
}

int arp_request(struct net_device *dev, __u32 ip)
{
	struct skbuff *skb;
	DBGPRINT(DBG_LEVEL_TRACE,"==>arp_request\n");
	__u32 tip = 0;
	if((ip&dev->ip_netmask.s_addr) == (dev->ip_host.s_addr & dev->ip_netmask.s_addr ) )
		tip = ip;
	else
		tip = dev->ip_gw.s_addr;
	skb = arp_create(dev,
					ARPOP_REQUEST,
					dev->ip_host.s_addr,
					tip,
					dev->hwaddr,
					NULL,
					NULL);
	if(skb){
		dev->linkoutput(skb, dev);
	}
	DBGPRINT(DBG_LEVEL_TRACE,"<==arp_request\n");
}
int arp_input(struct skbuff **pskb, struct net_device *dev)
{
	struct skbuff *skb = *pskb;

	__be32 ip = 0;
	DBGPRINT(DBG_LEVEL_TRACE,"==>arp_input\n");
	if(skb->tot_len < sizeof(struct sip_arphdr))
	{
		goto EXITarp_input;
	}
	ip = *(__be32*)(skb->nh.arph->ar_tip) ;
	if(ip == dev->ip_host.s_addr)
	{
		update_arp_entry(ip, dev->hwaddr);
	}

	switch(ntohs(skb->nh.arph->ar_op))
	{
		case ARPOP_REQUEST:
			{
				struct in_addr t_addr;
				t_addr.s_addr = *(unsigned int*)skb->nh.arph->ar_sip;
				DBGPRINT(DBG_LEVEL_ERROR,"ARPOP_REQUEST, FROM:%s\n",inet_ntoa(t_addr));

			arp_send(dev, 
					ARPOP_REPLY, 
					dev->ip_host.s_addr,
					*(__u32*)skb->nh.arph->ar_sip, 
					dev->hwaddr,
					skb->phy.ethh->h_source, 
					skb->nh.arph->ar_sha);
			arp_add_entry(*(__u32*)skb->nh.arph->ar_sip, skb->phy.ethh->h_source, ARP_ESTABLISHED);
			}
			break;
		case ARPOP_REPLY:
			arp_add_entry(*(__u32*)skb->nh.arph->ar_sip, skb->phy.ethh->h_source, ARP_ESTABLISHED);
			break;
	}
	DBGPRINT(DBG_LEVEL_TRACE,"<==arp_input\n");
EXITarp_input:
	return;
}


