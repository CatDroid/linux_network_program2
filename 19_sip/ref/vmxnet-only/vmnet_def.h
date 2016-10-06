/* **********************************************************
 * Copyright 2004 VMware, Inc.  All rights reserved. 
 * -- VMware Confidential
 * **********************************************************/

/*
 * vmnet_def.h 
 *
 *     - definitions which are (mostly) not vmxnet or vlance specific
 */

#ifndef _VMNET_DEF_H_
#define _VMNET_DEF_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMNIXMOD
#include "includeCheck.h"

/*
 * capabilities - not all of these are implemented in the virtual HW
 *                (eg VLAN support is in the virtual switch)  so even vlance 
 *                can use them
 */
#define VMNET_CAP_SG	        0x0001	/* Can do scatter-gather transmits. */
#define VMNET_CAP_IP4_CSUM      0x0002	/* Can checksum only TCP/UDP over IPv4. */
#define VMNET_CAP_HW_CSUM       0x0004	/* Can checksum all packets. */
#define VMNET_CAP_HIGH_DMA      0x0008	/* Can DMA to high memory. */
#define VMNET_CAP_TOE	        0x0010	/* Supports TCP/IP offload. */
#define VMNET_CAP_TSO	        0x0020	/* Supports TCP Segmentation offload */
#define VMNET_CAP_SW_TSO        0x0040	/* Supports SW TCP Segmentation */
#define VMNET_CAP_VMXNET_APROM  0x0080	/* Vmxnet APROM support */
#define VMNET_CAP_HW_TX_VLAN    0x0100  /* Can we do VLAN tagging in HW */
#define VMNET_CAP_HW_RX_VLAN    0x0200  /* Can we do VLAN untagging in HW */
#define VMNET_CAP_SW_VLAN       0x0400  /* Can we do VLAN tagging/untagging in SW */
#define VMNET_CAP_WAKE_PCKT_RCV 0x0800  /* Can wake on network packet recv? */
#define VMNET_CAP_ENABLE_INT_INLINE 0x1000  /* Enable Interrupt Inline */
#define VMNET_CAP_ENABLE_HEADER_COPY 0x2000  /* copy header for vmkernel */

#endif // _VMNET_DEF_H_
