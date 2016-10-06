#ifndef __SIP_SKBUFF_H__
#define __SIP_SKBUFF_H__

#include "sip_ether.h"
#include "sip_skbuff.h"
#include "sip_arp.h"
#include "sip_ip.h"
#include "sip_icmp.h"
#include "sip_tcp.h"
#include "sip_udp.h"
#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2
struct sip_tcphdr;
struct sip_udphdr;
struct sip_icmphdr;
struct sip_igmphdr;
struct sip_iphdr;
struct sip_arphdr;
struct sip_ethhdr;
struct net_device;
struct skbuff {
	/** next skbuff in singly linked skbuff chain */
	struct skbuff *next;

	union {
		struct sip_tcphdr		*tcph;
		struct sip_udphdr		*udph;
		struct sip_icmphdr	*icmph;
		struct sip_igmphdr	*igmph;
		__u8			*raw;
	} th;

	union {
		struct sip_iphdr		*iph;
		struct sip_arphdr		*arph;
		__u8			*raw;
	} nh;

	union {
		struct sip_ethhdr 		*ethh;
	  	__u8 			*raw;
	} phy;

	__be16				protocol;
	
	struct net_device  	*dev;
	__u32 				tot_len;
	__u32 				len;  
	
	__u8 csum;
	/** skbuff_type as u8_t instead of enum to save space */
	__u32 /*skbuff_type*/ type;

	/** misc flags */
	__u8 flags;

	/**
	* the reference count always equals the number of pointers
	* that refer to this skbuff. This can be pointers from an application,
	* the stack itself, or skbuff->next pointers from a chain.
	*/
	__u16 ref;
	__u8			local_df:1,
				cloned:1,
				ip_summed:2,
				nohdr:1,
				nfctinfo:3;

	/** pointer to the actual data in the buffer */	
	__u8		*head,
				*data,
				*tail,
				*end;


  
};

struct sip_sk_buff_head {
	/* These two members must be first. */
	struct skbuff	*next;
	struct skbuff	*prev;

	__u32		qlen;
};

#endif /*__SIP_SKBUFF_H__*/
