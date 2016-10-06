/* **********************************************************
 * Copyright 2004 VMware, Inc.  All rights reserved. 
 * -- VMware Confidential
 * **********************************************************/

/*
 * net_dist.h --
 *
 *      Networking headers.
 */

#ifndef _NET_DIST_H_
#define _NET_DIST_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

typedef uint32 Net_PortID;
/*
 * Set this to anything, and that value will never be assigned as a 
 * PortID for a valid port, but will be assigned in most error cases.
 */
#define NET_INVALID_PORT_ID      0

/*
 * Set this to the largest size of private implementation data expected
 * to be embedded in a pkt by device driver wrapper modules
 * (e.g. sizeof(esskaybee_or_whatever))
 */
#ifdef VM_X86_64
#define NET_MAX_IMPL_PKT_OVHD   (480)
#else
#define NET_MAX_IMPL_PKT_OVHD   (280)
#endif

/*
 * Set this to the largest alignment requirements made by various driver
 * wrappers. Unfortunately linux guarantees space for 16 byte alignment
 * from the drivers, although most drivers don't use it (none?).
 */
#define NET_MAX_IMPL_ALIGN_OVHD (16)

/*
 * Set this to the size of the largest object that a driver will embed in
 * the buffer aside from the frame data, currently the e100 has a 32 byte
 * struct that it puts there.
 */ 
#define NET_MAX_DRV_PKT_OVHD    (32) 

/*
 * Set this to the largest alignment overhead connsumed by the various
 * drivers for alignment purposes.  Many of the drivers want an extra 
 * 2 bytes for aligning iphdr, and the 3c90x wants 64 byte aligned dma.
 */
#define NET_MAX_DRV_ALIGN_OVHD  (64+2)


#endif
