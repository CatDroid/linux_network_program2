/* **********************************************************
 * Copyright 1998 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

#ifndef __VMXNETINT_H__
#define __VMXNETINT_H__

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "return_status.h"
#include "net_dist.h"

#define VMXNET_CHIP_NAME "vmxnet ether"

#define CRC_POLYNOMIAL_LE 0xedb88320UL  /* Ethernet CRC, little endian */

#define PKT_BUF_SZ			1536

/* Largest address able to be shared between the driver and the device */
#define SHARED_MEM_MAX 0xFFFFFFFF

typedef enum Vmxnet_TxStatus {
   VMXNET_CALL_TRANSMIT,
   VMXNET_DEFER_TRANSMIT,
   VMXNET_STOP_TRANSMIT
} Vmxnet_TxStatus;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
#   define MODULE_PARM(var, type)
#   define net_device_stats enet_statistics
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,14))
#   define net_device device
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43))
static inline void
netif_start_queue(struct device *dev)
{
   clear_bit(0, &dev->tbusy);
}

static inline void
netif_stop_queue(struct device *dev)
{
   set_bit(0, &dev->tbusy);
}

static inline int
netif_queue_stopped(struct device *dev)
{
   return test_bit(0, &dev->tbusy);
}

static inline void
netif_wake_queue(struct device *dev)
{
   clear_bit(0, &dev->tbusy);
   mark_bh(NET_BH);
}

static inline int
netif_running(struct device *dev)
{
   return dev->start == 0;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0))
#   define le16_to_cpu(x) ((__u16)(x))
#   define le32_to_cpu(x) ((__u32)(x))
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0))
#   define compat_kfree_skb(skb, type) kfree_skb(skb, type)
#   define compat_dev_kfree_skb(skb, type) dev_kfree_skb(skb, type)
#   define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb(skb, type)
#   define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb(skb, type)
#else
#   define compat_kfree_skb(skb, type) kfree_skb(skb)
#   define compat_dev_kfree_skb(skb, type) dev_kfree_skb(skb)
#   if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43))
#      define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb(skb)
#      define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb(skb)
#   else
#      define compat_dev_kfree_skb_any(skb, type) dev_kfree_skb_any(skb)
#      define compat_dev_kfree_skb_irq(skb, type) dev_kfree_skb_irq(skb)
#   endif
#endif

#if defined(BUG_ON)
#define VMXNET_ASSERT(cond) BUG_ON(!(cond))
#else
#define VMXNET_ASSERT(cond)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19) && defined(CHECKSUM_HW)
#   define VM_CHECKSUM_PARTIAL     CHECKSUM_HW
#   define VM_CHECKSUM_UNNECESSARY CHECKSUM_UNNECESSARY
#else
#   define VM_CHECKSUM_PARTIAL     CHECKSUM_PARTIAL
#   define VM_CHECKSUM_UNNECESSARY CHECKSUM_UNNECESSARY
#endif

struct Vmxnet_TxBuf {
   struct sk_buff *skb;
   char    sgForLinear; /* the sg entry mapping the linear part 
                         * of the skb, -1 means this tx entry only
                         * mapps the frags of the skb
                         */ 
   char    firstSgForFrag;   /* the first sg entry mapping the frags */
   Bool    eop;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18) || defined(VMW_SKB_LINEARIZE_2618)
#   define compat_skb_linearize(skb) skb_linearize((skb))
#else
#   define compat_skb_linearize(skb) skb_linearize((skb), GFP_ATOMIC)
#endif

/*
 * Private data area, pointed to by priv field of our struct net_device.
 * dd field is shared with the lower layer.
 */
typedef struct Vmxnet_Private {
   Vmxnet2_DriverData	       *dd;
   const char 		       *name;
   struct net_device_stats	stats;
   struct sk_buff	       *rxSkbuff[VMXNET2_MAX_NUM_RX_BUFFERS];
   struct sk_buff             **rxRingBuffPtr[VMXNET2_MAX_NUM_RX_BUFFERS];
   struct Vmxnet_TxBuf          txBufInfo[VMXNET2_MAX_NUM_TX_BUFFERS];
   spinlock_t                   txLock;
   int				numTxPending;
   unsigned int			numRxBuffers;
   unsigned int			numTxBuffers;
   Vmxnet2_RxRingEntry         *rxRing;
   Vmxnet2_TxRingEntry         *txRing;

   Bool				devOpen;
   Net_PortID			portID;

   uint32                       capabilities;
   uint32                       features;

   Bool                         zeroCopyTx;
   Bool                         partialHeaderCopyEnabled;
   Bool                         tso;
   
   Bool                         morphed;           // Indicates whether adapter is morphed
   void                        *ddAllocated;
   char                        *txBufferStartRaw;
   char                        *txBufferStart;
   struct pci_dev              *pdev;
} Vmxnet_Private;

#define TMC_FRAG_LINEAR   -1
// these fields are updated for each tx entry
struct Vmxnet_TxMapContext {
   int nextFrag;           // TMC_FRAG_LINEAR means the linear portion
   Bool giantLinear;       // the pkt has a giant linear part
   unsigned int offset;    // only valid when nextFrag == TMC_FRAG_LINEAR
   unsigned int mappedLen; // total # of bytes mapped for the tx entry
};

#endif /* __VMXNETINT_H__ */
