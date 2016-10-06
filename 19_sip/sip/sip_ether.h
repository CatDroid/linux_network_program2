#ifndef __SIP_ETHER_H__
#define __SIP_ETHER_H__

#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/
#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
#define ETH_FRAME_LEN	1514		/* Max. octets in frame sans FCS */

#define ETH_P_ALL	0x0003		/* Every packet (be careful!!!) */

/*
 *	This is an Ethernet frame header.
 */ 
struct sip_ethhdr {
	__u8	h_dest[ETH_ALEN];	/* destination eth addr	*/
	__u8	h_source[ETH_ALEN];	/* source ether addr	*/
	__be16	h_proto;		/* packet type ID field	*/
} ;
struct skbuff;
struct net_device {
	char				name[IFNAMSIZ];

	/** IP address configuration in network byte order */
	struct in_addr	ip_host;	/* Internet address		*/
	struct in_addr 	ip_netmask;
	struct in_addr 	ip_broadcast;
	struct in_addr 	ip_gw;
	struct in_addr 	ip_dest;
	__u16			type;
	
	

	/** This function is called by the network device driver
	*  to pass a packet up the TCP/IP stack. */
	__u8 (* input)(struct skbuff *skb, struct net_device *dev);
	/** This function is called by the IP module when it wants
	*  to send a packet on the interface. This function typically
	*  first resolves the hardware address, then sends the packet. */
	__u8 (* output)(struct skbuff *skb, struct net_device *dev);
	/** This function is called by the ARP module when it wants
	*  to send a packet on the interface. This function outputs
	*  the skbuff as-is on the link medium. */
	__u8 (* linkoutput)(struct skbuff *skb, struct net_device *dev);
	/** number of bytes used in hwaddr */
	__u8 hwaddr_len;
	/** link level hardware address of this interface */
	__u8 hwaddr[ETH_ALEN];
	__u8 hwbroadcast[ETH_ALEN];
	/** maximum transfer unit (in bytes) */
	__u8 mtu;
	/** flags (see NETIF_FLAG_ above) */
	__u8 flags;

	int s;
	struct sockaddr to;
};

#endif /*__SIP_ETHER_H__*/

