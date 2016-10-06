#ifndef __SIP_ARP_H__
#define __SIP_ARP_H__

#define	ARPOP_REQUEST	1		/* ARP request			*/
#define	ARPOP_REPLY	2		/* ARP reply			*/
#define 	ETH_P_802_3	0x0001
#define 	ARP_TABLE_SIZE 10
#define 	ARP_LIVE_TIME	20


enum arp_status{
	ARP_EMPTY,	ARP_PADDING, ARP_ESTABLISHED
};

struct arpt_arp{
	__u32	ipaddr;
	__u8	ethaddr[ETH_ALEN];
  	time_t 	ctime;
	enum arp_status status;
};


struct sip_arphdr
{
	__be16	ar_hrd;		/* format of hardware address	*/
	__be16	ar_pro;		/* format of protocol address	*/
	__u8	ar_hln;		/* length of hardware address	*/
	__u8	ar_pln;		/* length of protocol address	*/
	__be16	ar_op;		/* ARP opcode (command)		*/
	 /*
	  *	 Ethernet looks like this : This bit is variable sized however...
	  */
	__u8 ar_sha[ETH_ALEN];	/* sender hardware address	*/
	__u8 ar_sip[4];		/* sender IP address		*/
	__u8 ar_tha[ETH_ALEN];	/* target hardware address	*/
	__u8 ar_tip[4];		/* target IP address		*/
};




#endif /*__SIP_ARP_H__*/
