#ifndef __SIP_UDP_H__
#define __SIP_UDP_H__
#define UDP_FLAGS_NOCHKSUM 0x01U
#define UDP_FLAGS_UDPLITE  0x02U
#define UDP_FLAGS_CONNECTED  0x04U
struct sip_udphdr {
	__be16	source;
	__be16	dest;
	__u16	len;
	__be16	check;
};

struct ip_pcb 
{
	/* Common members of all PCB types */
	/* ip addresses in network byte order */
	struct in_addr ip_local; 
	struct in_addr ip_remote;
	/* Socket options */  
	__u16 so_options;     
	/* Type Of Service */ 
	__u8 tos;              
	/* Time To Live */     
	__u8 ttl  ;            
	/* link layer address resolution hint */ 
	__u8 addr_hint;
};
struct udp_pcb {
	/* Common members of all PCB types */
	/* ip addresses in network byte order */
	struct in_addr ip_local; 
	struct in_addr ip_remote;
	/* Socket options */  
	__u16 so_options;     
	/* Type Of Service */ 
	__u8 tos;              
	/* Time To Live */     
	__u8 ttl ;             
	/* link layer address resolution hint */ 
	__u8 addr_hint;

	

	/* Protocol specific PCB members */
	struct udp_pcb *next;
	struct udp_pcb *prev;

	__u8 flags;
	/* ports are in host byte order */
	__u16 port_local;
	__u16 port_remote;


#if LWIP_UDPLITE
  	/* used for UDP_LITE only */
  	__u16 chksum_len_rx, chksum_len_tx;
#endif /* LWIP_UDPLITE */
	/* receive callback function
	* addr and port are in same byte order as in the pcb
	* The callback is responsible for freeing the skbuff
	* if it's not used any more.
	*
	* @param arg user supplied argument (udp_pcb.recv_arg)
	* @param pcb the udp_pcb which received data
	* @param p the packet buffer that was received
	* @param addr the remote IP address from which the packet was received
	* @param port the remote port from which the packet was received
	*/
	void (* recv)(void *arg, 
				struct udp_pcb *pcb, 
				struct skbuff *skb,
				struct in_addr *addr, 
				__u16 port);
	/* user-supplied argument for the recv callback */
	void *recv_arg;  
	struct sock *conn;
};

#endif /*__SIP_UDP_H__*/
