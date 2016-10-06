#include "sip.h"
#include "sip_sock.h"
#define UDP_HTABLE_SIZE 128
static struct udp_pcb *udp_pcbs[UDP_HTABLE_SIZE];
static __u16 found_a_port()
{
	static __u32 index = 0x0;
	index ++;
	return (__u16)(index&0xFFFF);
}


/**
 * Create a UDP PCB.
 *
 * @return The UDP PCB which was created. NULL if the PCB data structure
 * could not be allocated.
 *
 * @see udp_remove()
 */
struct udp_pcb *SIP_UDPNew(void)
{
	struct udp_pcb *pcb;
	pcb = (struct udp_pcb *)malloc(sizeof(struct udp_pcb *));
	/* could allocate UDP PCB? */
	if (pcb != NULL) 
	{
		/* UDP Lite: by initializing to all zeroes, chksum_len is set to 0
		* which means checksum is generated over the whole datagram per default
		* (recommended as default by RFC 3828). */
		/* initialize PCB to all zeroes */
		memset(pcb, 0, sizeof(struct udp_pcb));
		pcb->ttl = 255;
	}
	
	return pcb;
}

/**
 * Remove an UDP PCB.
 *
 * @param pcb UDP PCB to be removed. The PCB is removed from the list of
 * UDP PCB's and the data structure is freed from memory.
 *
 * @see udp_new()
 */
void SIP_UDPRemove(struct udp_pcb *pcb)
{
	struct udp_pcb *pcb_t;
	int i = 0;

	if(!pcb)
	{
		return;
	}

	pcb_t = udp_pcbs[pcb->port_local%UDP_HTABLE_SIZE];
	if(!pcb_t)
	{
		;
	}
	else if(pcb_t == pcb)
	{
		udp_pcbs[pcb->port_local%UDP_HTABLE_SIZE] = pcb_t->next;
	}
	else
	{
		for (; pcb_t->next != NULL; pcb_t = pcb_t->next) 
		{
			/* find pcb in udp_pcbs list */
			if (pcb_t->next == pcb) 
			{
				/* remove pcb from list */
				pcb_t->next = pcb->next;
			}
		}
	}
	
	free(pcb);
}


/**
 * Bind an UDP PCB.
 *
 * @param pcb UDP PCB to be bound with a local address ipaddr and port.
 * @param ipaddr local IP address to bind with. Use IP_ADDR_ANY to
 * bind to all local interfaces.
 * @param port local UDP port to bind with. Use 0 to automatically bind
 * to a random port between UDP_LOCAL_PORT_RANGE_START and
 * UDP_LOCAL_PORT_RANGE_END.
 *
 * ipaddr & port are expected to be in the same byte order as in the pcb.
 *
 * @return lwIP error code.
 * - 0. Successful. No error occured.
 * - -1. The specified ipaddr and port are already bound to by
 * another UDP PCB.
 *
 * @see udp_disconnect()
 */
int SIP_UDPBind(struct udp_pcb *pcb, 
			struct in_addr *ipaddr,
			__u16 port)
{
	struct udp_pcb *ipcb;
	__u8 rebind;

	rebind = 0;
	/* Check for double bind and rebind of the same pcb */
	for (ipcb = udp_pcbs[port&(UDP_HTABLE_SIZE-1)]; ipcb != NULL; ipcb = ipcb->next) 
	{
		/* is this UDP PCB already on active list? */
		if (pcb == ipcb) 
		{
			/* pcb may occur at most once in active list */
			/* pcb already in list, just rebind */
			rebind = 1;
		}

		/* this code does not allow upper layer to share a UDP port for
		listening to broadcast or multicast traffic (See SO_REUSE_ADDR and
		SO_REUSE_PORT under *BSD). TODO: See where it fits instead, OR
		combine with implementation of UDP PCB flags. Leon Woestenberg. */
	}

	pcb->ip_local.s_addr= ipaddr->s_addr;

	/* no port specified? */
	if (port == 0) 
	{
#define UDP_PORT_RANGE_START 4096
#define UDP_PORT_RANGE_END   0x7fff
		port = found_a_port();
		ipcb = udp_pcbs[port];
		while ((ipcb!=NULL)&&(port != UDP_PORT_RANGE_END) )
		{
			if (ipcb->port_local == port) 
			{
				/* port is already used by another udp_pcb */
				port = found_a_port();
				/* restart scanning all udp pcbs */
				ipcb = udp_pcbs[port];
			}
			else
			{	
				/* go on with next udp pcb */
				ipcb = ipcb->next;
			}
		}

		if (ipcb != NULL) 
		{
			/* no more ports available in local range */
			return -1;
		}
	}

	pcb->port_local = port;
	/* pcb not active yet? */
	if (rebind == 0) 
	{
		/* place the PCB on the active list if not already there */
		pcb->next = udp_pcbs[port];
		udp_pcbs[port] = pcb;
	}

	return 0;
}

/**
 * Connect an UDP PCB.
 *
 * This will associate the UDP PCB with the remote address.
 *
 * @param pcb UDP PCB to be connected with remote address ipaddr and port.
 * @param ipaddr remote IP address to connect with.
 * @param port remote UDP port to connect with.
 *
 * @return lwIP error code
 *
 * ipaddr & port are expected to be in the same byte order as in the pcb.
 *
 * The udp pcb is bound to a random local port if not already bound.
 *
 * @see udp_disconnect()
 */
int
SIP_UDPConnect(struct udp_pcb *pcb, 
					struct in_addr *ipaddr, 
					__u16 port)
{
	struct udp_pcb *ipcb;

	if (pcb->port_local == 0) 
	{
		int err = SIP_UDPBind(pcb, &pcb->ip_local, 0);
		if (err != 0)
			return err;
	}

	pcb->ip_remote.s_addr = ipaddr->s_addr;
	pcb->port_remote = port;
	pcb->flags |= UDP_FLAGS_CONNECTED;
	/** TODO: this functionality belongs in upper layers */

	/* Insert UDP PCB into the list of active UDP PCBs. */
	for (ipcb = udp_pcbs[pcb->port_local]; ipcb != NULL; ipcb = ipcb->next) 
	{
		if (pcb == ipcb) 
		{
			/* already on the list, just return */
			return 0;
		}
	}

	/* PCB not yet on the list, add PCB now */
	pcb->next = udp_pcbs[pcb->port_local];
	udp_pcbs[pcb->port_local] = pcb;

	return 0;
}


/**
 * Disconnect a UDP PCB
 *
 * @param pcb the udp pcb to disconnect.
 */
void SIP_UDPDisconnect(struct udp_pcb *pcb)
{
	/* reset remote address association */
	pcb->ip_remote.s_addr = -1;
	pcb->port_remote = 0;
	/* mark PCB as unconnected */
	pcb->flags &= ~UDP_FLAGS_CONNECTED;
}



/**
 * Send data to a specified address using UDP.
 *
 * @param pcb UDP PCB used to send the data.
 * @param p chain of skbuff's to be sent.
 * @param dst_ip Destination IP address.
 * @param dst_port Destination UDP port.
 *
 * dst_ip & dst_port are expected to be in the same byte order as in the pcb.
 *
 * If the PCB already has a remote address association, it will
 * be restored after the data is sent.
 * 
 * @return lwIP error code (@see udp_send for possible error codes)
 *
 * @see udp_disconnect() udp_send()
 */
int SIP_UDPSendTo(struct net_device *dev,struct udp_pcb *pcb, struct skbuff *skb,
  struct in_addr *dst_ip, __u16 dst_port)
{
	struct sip_udphdr *udphdr;
	struct in_addr *src_ip;
	int err;
	struct skbuff *q; /* q will be sent down the stack */

	/* if the PCB is not yet bound to a port, bind it here */
	if (pcb->port_local == 0) 
	{
		err = SIP_UDPBind(pcb, &pcb->ip_local, pcb->port_local);
		if (err != 0) 
		{
			return err;
		}
	}

	/* q now represents the packet to be sent */
	udphdr = skb->th.udph;
	udphdr->source = htons(pcb->port_local);
	udphdr->dest = htons(dst_port);
	/* in UDP, 0 checksum means 'no checksum' */
	udphdr->check= 0x0000; 

	/* PCB local address is IP_ANY_ADDR? */
	if (pcb->ip_local.s_addr == 0) 
	{
		/* use outgoing network interface IP address as source address */
		src_ip->s_addr = dev->ip_host.s_addr;
	} 
	else 
	{
		/* use UDP PCB local IP address as source address */
		src_ip = &(pcb->ip_local);
	}

	udphdr->len = htons(q->tot_len);
	/* calculate checksum */
	if ((pcb->flags & UDP_FLAGS_NOCHKSUM) == 0) 
	{
		udphdr->check = SIP_ChksumPseudo(skb, src_ip, dst_ip, IPPROTO_UDP, q->tot_len);
		/* chksum zero must become 0xffff, as zero means 'no checksum' */
		if (udphdr->check == 0x0000) udphdr->check = 0xffff;
	}

	/* output to IP */
	err = SIP_UDPSendOutput(skb, src_ip, dst_ip, pcb->ttl, pcb->tos, IPPROTO_UDP);    

	return err;

}
int  SIP_UDPSend(struct net_device *dev,struct udp_pcb *pcb, struct skbuff *skb)
{
 	/* send to the packet using remote ip and port stored in the pcb */
	return SIP_UDPSendTo(dev, pcb,skb, &pcb->ip_remote, pcb->port_remote);
}

int SIP_UDPInput(struct net_device *dev, struct skbuff *skb)
{
	struct skbuff *recvl ;
	__u16 port = ntohs(skb->th.udph->dest);
	
	struct udp_pcb *upcb = NULL;
	for(upcb = udp_pcbs[port%UDP_HTABLE_SIZE]; upcb != NULL; upcb = upcb->next)
	{
		if(upcb->port_local== port)
			break;
	}

	if(!upcb)
		return 0;

	struct sock *conn = upcb->conn;
	if(!conn)
		return 1;

	recvl = conn->skb_recv;
	if(!recvl)
	{
		conn->skb_recv = skb;
		skb->next = NULL;
	}
	else
	{
		for(; recvl->next != NULL; upcb = upcb->next)
			;
		recvl->next = skb;
		skb->next = NULL;
	}
	
}

int SIP_UDPSendOutput(struct net_device *dev, struct skbuff *skb,struct udp_pcb *pcb,
	struct in_addr *src, struct in_addr *dest)
{
	ip_output(dev,skb, src, dest, pcb->ttl, pcb->tos, IPPROTO_UDP);    
}
