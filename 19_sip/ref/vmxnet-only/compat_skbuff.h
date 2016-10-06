/* **********************************************************
 * Copyright (C) 2007 VMware, Inc.  All Rights Reserved. -- VMware Confidential
 * **********************************************************/

#ifndef __COMPAT_SKBUFF_H__
#   define __COMPAT_SKBUFF_H__

#include <linux/skbuff.h>

/*
 * When transition from mac/nh/h to skb_* accessors was made, also SKB_WITH_OVERHEAD
 * was introduced.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22) || \
   (LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 21) && defined(SKB_WITH_OVERHEAD))
#define compat_skb_mac_header(skb)         skb_mac_header(skb)
#define compat_skb_network_header(skb)     skb_network_header(skb)
#define compat_skb_network_offset(skb)     skb_network_offset(skb)
#define compat_skb_transport_header(skb)   skb_transport_header(skb)
#define compat_skb_transport_offset(skb)   skb_transport_offset(skb)
#define compat_skb_network_header_len(skb) skb_network_header_len(skb)
#define compat_skb_tail_pointer(skb)       skb_tail_pointer(skb)
#define compat_skb_end_pointer(skb)        skb_end_pointer(skb)
#define compat_skb_ip_header(skb)          ((struct iphdr *)skb_network_header(skb))
#define compat_skb_tcp_header(skb)         ((struct tcphdr *)skb_transport_header(skb))
#define compat_skb_reset_mac_header(skb)   skb_reset_mac_header(skb)
#define compat_skb_set_network_header(skb, off)   skb_set_network_header(skb, off)
#define compat_skb_set_transport_header(skb, off) skb_set_transport_header(skb, off)
#else
#define compat_skb_mac_header(skb)         (skb)->mac.raw
#define compat_skb_network_header(skb)     (skb)->nh.raw
#define compat_skb_network_offset(skb)     ((skb)->nh.raw - (skb)->data)
#define compat_skb_transport_header(skb)   (skb)->h.raw
#define compat_skb_transport_offset(skb)   ((skb)->h.raw - (skb)->data)
#define compat_skb_network_header_len(skb) ((skb)->h.raw - (skb)->nh.raw)
#define compat_skb_tail_pointer(skb)       (skb)->tail
#define compat_skb_end_pointer(skb)        (skb)->end
#define compat_skb_ip_header(skb)          (skb)->nh.iph
#define compat_skb_tcp_header(skb)         (skb)->h.th
#define compat_skb_reset_mac_header(skb)   ((skb)->mac.raw = (skb)->data)
#define compat_skb_set_network_header(skb, off)   ((skb)->nh.raw = (skb)->data + (off))
#define compat_skb_set_transport_header(skb, off) ((skb)->h.raw = (skb)->data + (off))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#define compat_skb_csum_offset(skb)        (skb)->csum_offset
#else
#define compat_skb_csum_offset(skb)        (skb)->csum
#endif

/*
 * Note that compat_skb_csum_start() has semantic different from kernel's csum_start:
 * kernel's skb->csum_start is offset between start of checksummed area and start of
 * complete skb buffer, while our compat_skb_csum_start(skb) is offset from start
 * of packet itself.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
#define compat_skb_csum_start(skb)         ((skb)->csum_start - skb_headroom(skb))
#else
#define compat_skb_csum_start(skb)         compat_skb_transport_offset(skb)
#endif

#endif /* __COMPAT_SKBUFF_H__ */
