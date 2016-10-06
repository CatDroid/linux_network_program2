#include "sip.h"

static void icmp_echo(struct net_device *dev, struct skbuff *skb)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_echo\n");
	struct sip_icmphdr *icmph = skb->th.icmph;
	struct sip_iphdr *iph = skb->nh.iph;
	DBGPRINT(DBG_LEVEL_NOTES,"tot_len:%d\n",skb->tot_len);
	if(IP_IS_BROADCAST(dev, skb->nh.iph->daddr) 
		|| IP_IS_MULTICAST(skb->nh.iph->daddr)){
		goto EXITicmp_echo;
	}

	icmph->type = ICMP_ECHOREPLY;
	if(icmph->checksum >= htons(0xFFFF-(ICMP_ECHO << 8))){
		icmph->checksum += htons(ICMP_ECHO<<8 )+1;
	}else{
		icmph->checksum += htons(ICMP_ECHO<<8);
	}

	__be32 dest = skb->nh.iph->saddr;

	ip_output(dev,skb,&dev->ip_host.s_addr,&dest, 255, 0, IPPROTO_ICMP);
	
EXITicmp_echo:
	DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_echo\n");
	return ;
}
static void icmp_discard(struct net_device *dev, struct skbuff *skb)
{
DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_discard\n");
DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_discard\n");
}

static void icmp_unreach(struct net_device *dev, struct skbuff *skb)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_unreach\n");
	#if 0
  struct skbuff *q;
  struct ip_hdr *iphdr;
  struct icmp_dur_hdr *idur;

  /* ICMP header + IP header + 8 bytes of data */
  q = skbuff_alloc(PBUF_IP, sizeof(struct icmp_dur_hdr) + IP_HLEN + ICMP_DEST_UNREACH_DATASIZE,
                 PBUF_RAM);
  if (q == NULL) {
    LWIP_DEBUGF(ICMP_DEBUG, ("icmp_dest_unreach: failed to allocate skbuff for ICMP packet.\n"));
    return;
  }
  LWIP_ASSERT("check that first skbuff can hold icmp message",
             (q->len >= (sizeof(struct icmp_dur_hdr) + IP_HLEN + ICMP_DEST_UNREACH_DATASIZE)));

  iphdr = p->payload;

  idur = q->payload;
  ICMPH_TYPE_SET(idur, ICMP_DUR);
  ICMPH_CODE_SET(idur, t);

  SMEMCPY((u8_t *)q->payload + sizeof(struct icmp_dur_hdr), p->payload,
          IP_HLEN + ICMP_DEST_UNREACH_DATASIZE);

  /* calculate checksum */
  idur->chksum = 0;
  idur->chksum = inet_chksum(idur, q->len);
  ICMP_STATS_INC(icmp.xmit);
  /* increase number of messages attempted to send */
  snmp_inc_icmpoutmsgs();
  /* increase number of destination unreachable messages attempted to send */
  snmp_inc_icmpoutdestunreachs();

  ip_output(q, NULL, &(iphdr->src), ICMP_TTL, 0, IP_PROTO_ICMP);
  skb_free(q);
  #endif
	DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_unreach\n");
}

static void icmp_redirect(struct net_device *dev, struct skbuff *skb)
{
DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_redirect\n");
DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_redirect\n");
}

static void icmp_timestamp(struct net_device *dev, struct skbuff *skb)
{
DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_timestamp\n");
DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_timestamp\n");
}

static void icmp_address(struct net_device *dev, struct skbuff *skb)
{
DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_address\n");
DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_address\n");
}

static void icmp_address_reply(struct net_device *dev, struct skbuff *skb)
{
DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_address_reply\n");
DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_address_reply\n");
}
/*
 *	This table is the definition of how we handle ICMP.
 */
static const struct icmp_control icmp_pointers[NR_ICMP_TYPES + 1] = {
	[ICMP_ECHOREPLY] = {
		.output_entry = ICMP_MIB_OUTECHOREPS,
		.input_entry = ICMP_MIB_INECHOREPS,
		.handler = icmp_discard,
	},
	[1] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[2] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_DEST_UNREACH] = {
		.output_entry = ICMP_MIB_OUTDESTUNREACHS,
		.input_entry = ICMP_MIB_INDESTUNREACHS,
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_SOURCE_QUENCH] = {
		.output_entry = ICMP_MIB_OUTSRCQUENCHS,
		.input_entry = ICMP_MIB_INSRCQUENCHS,
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_REDIRECT] = {
		.output_entry = ICMP_MIB_OUTREDIRECTS,
		.input_entry = ICMP_MIB_INREDIRECTS,
		.handler = icmp_redirect,
		.error = 1,
	},
	[6] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[7] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_ECHO] = {
		.output_entry = ICMP_MIB_OUTECHOS,
		.input_entry = ICMP_MIB_INECHOS,
		.handler = icmp_echo,
	},
	[9] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[10] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_INERRORS,
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_TIME_EXCEEDED] = {
		.output_entry = ICMP_MIB_OUTTIMEEXCDS,
		.input_entry = ICMP_MIB_INTIMEEXCDS,
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_PARAMETERPROB] = {
		.output_entry = ICMP_MIB_OUTPARMPROBS,
		.input_entry = ICMP_MIB_INPARMPROBS,
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_TIMESTAMP] = {
		.output_entry = ICMP_MIB_OUTTIMESTAMPS,
		.input_entry = ICMP_MIB_INTIMESTAMPS,
		.handler = icmp_timestamp,
	},
	[ICMP_TIMESTAMPREPLY] = {
		.output_entry = ICMP_MIB_OUTTIMESTAMPREPS,
		.input_entry = ICMP_MIB_INTIMESTAMPREPS,
		.handler = icmp_discard,
	},
	[ICMP_INFO_REQUEST] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_DUMMY,
		.handler = icmp_discard,
	},
 	[ICMP_INFO_REPLY] = {
		.output_entry = ICMP_MIB_DUMMY,
		.input_entry = ICMP_MIB_DUMMY,
		.handler = icmp_discard,
	},
	[ICMP_ADDRESS] = {
		.output_entry = ICMP_MIB_OUTADDRMASKS,
		.input_entry = ICMP_MIB_INADDRMASKS,
		.handler = icmp_address,
	},
	[ICMP_ADDRESSREPLY] = {
		.output_entry = ICMP_MIB_OUTADDRMASKREPS,
		.input_entry = ICMP_MIB_INADDRMASKREPS,
		.handler = icmp_address_reply,
	},
};

int icmp_reply(struct net_device *dev, struct skbuff *skb)
{

}
/*
 *	Deal with incoming ICMP packets.
 */
int icmp_input(struct net_device *dev, struct skbuff *skb)
{
	DBGPRINT(DBG_LEVEL_TRACE,"==>icmp_input\n");
	struct sip_icmphdr *icmph;

	switch (skb->ip_summed) {
	case CHECKSUM_NONE:
		skb->csum = 0;
		if (cksum(skb->phy.raw, 0)){
			DBGPRINT(DBG_LEVEL_ERROR, "icmp_checksum error\n");
			goto drop;
		}
		break;
	default:
		break;
	}

	unsigned short csum = cksum(skb->th.raw, ntohs(skb->nh.iph->tot_len)-skb->nh.iph->ihl*4);
	DBGPRINT(DBG_LEVEL_NOTES,"ICMP check sum value is:%u\n",csum);

	icmph = skb->th.icmph;

	/*
	 *	18 is the highest 'known' ICMP type. Anything else is a mystery
	 *
	 *	RFC 1122: 3.2.2  Unknown ICMP messages types MUST be silently
	 *		  discarded.
	 */
	if (icmph->type > NR_ICMP_TYPES)
		goto drop;


	/*
	 *	Parse the ICMP message
	 */
	DBGPRINT(DBG_LEVEL_NOTES,"tot_len:%d\n",skb->tot_len);
 	icmp_pointers[icmph->type].handler(dev,skb);
normal:
	DBGPRINT(DBG_LEVEL_TRACE,"<==icmp_input\n");
	return 0;	

drop:
	skb_free(skb);
	goto normal;
}





