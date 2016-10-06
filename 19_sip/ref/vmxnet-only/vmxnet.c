/* **********************************************************
 * Copyright 1999 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/* 
 * vmxnet.c: A virtual network driver for VMware.
 */

#include "driver-config.h"

#include <linux/module.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif
   
#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_pci.h"
#include "compat_init.h"
#include <asm/dma.h>
#include <asm/page.h>
#include <asm/uaccess.h>

#include "compat_netdevice.h"
#include "compat_skbuff.h"
#include <linux/etherdevice.h>
#include "compat_ioport.h"
#ifndef KERNEL_2_1
#   include <linux/delay.h>
#endif
#include "compat_interrupt.h"

#ifndef VMX86_TOOLS
#define VMX86_TOOLS
#endif
#include "vm_basic_types.h"
#include "vmnet_def.h"
#include "vmxnet_def.h"
#include "vmxnet2_def.h"
#include "vm_device_version.h"
#include "vmxnetInt.h"
#include "net.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
#include "ethtool_compat.h"
#endif

static int vmxnet_debug = 1;

#define VMXNET_WATCHDOG_TIMEOUT (5 * HZ) 

#if defined(CONFIG_NET_POLL_CONTROLLER) || defined(HAVE_POLL_CONTROLLER)
#define VMW_HAVE_POLL_CONTROLLER
#endif

static int vmxnet_open(struct net_device *dev);
static int vmxnet_start_tx(struct sk_buff *skb, struct net_device *dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t vmxnet_interrupt(int irq, void *dev_id, 
					   struct pt_regs * regs);
#else
static compat_irqreturn_t vmxnet_interrupt(int irq, void *dev_id);
#endif
#ifdef VMW_HAVE_POLL_CONTROLLER
static void vmxnet_netpoll(struct net_device *dev);
#endif
static int vmxnet_close(struct net_device *dev);
static void vmxnet_set_multicast_list(struct net_device *dev);
static int vmxnet_set_mac_address(struct net_device *dev, void *addr);
static struct net_device_stats *vmxnet_get_stats(struct net_device *dev);

static int vmxnet_probe_device(struct pci_dev *pdev, const struct pci_device_id *id);
static void vmxnet_remove_device(struct pci_dev *pdev);

#ifdef MODULE
static int debug = -1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
   module_param(debug, int, 0444);
#else
   MODULE_PARM(debug, "i");
#endif
#endif

#ifdef VMXNET_DO_ZERO_COPY
#undef VMXNET_DO_ZERO_COPY
#endif

#if defined(MAX_SKB_FRAGS) && ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,18) ) && ( LINUX_VERSION_CODE != KERNEL_VERSION(2, 6, 0) )
#define VMXNET_DO_ZERO_COPY 
#endif

#ifdef VMXNET_DO_ZERO_COPY
#include <net/checksum.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>

/* Tx buffer size that we need for copying header*/
#define TX_PKT_HEADER_SIZE      128

/* Constants used for Zero Copy Tx */
#define ETHERNET_HEADER_SIZE            14
#define VLAN_TAG_LENGTH                4
#define ETH_FRAME_TYPE_LOCATION        12
#define ETH_TYPE_VLAN_TAG              0x0081 /* in little endian */
#define ETH_TYPE_IP                    0x0008 /* in little endian */

#define PKT_OF_PROTO(skb, type) \
   (*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) == (type) || \
    (*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) == ETH_TYPE_VLAN_TAG && \
     *(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION + VLAN_TAG_LENGTH) == (type)))

#define PKT_OF_IPV4(skb) PKT_OF_PROTO(skb, ETH_TYPE_IP)

#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define VMXNET_GET_LO_ADDR(dma)   ((uint32)(dma))
#define VMXNET_GET_HI_ADDR(dma)   ((uint16)(((uint64)(dma)) >> 32))
#define VMXNET_GET_DMA_ADDR(sge)  ((dma_addr_t)((((uint64)(sge).addrHi) << 32) | (sge).addrLow))

/* 2 for the copy of headers made by backend, 1 in case the linear part is not page aligned */
#define VMXNET_MAX_LINEAR        (13 * PAGE_SIZE)
#endif // VMXNET_DO_ZERO_COPY

#ifdef VMXNET_DEBUG
#define VMXNET_LOG(msg...) printk(KERN_ERR msg)
#else 
#define VMXNET_LOG(msg...)
#endif // VMXNET_DEBUG

/* Data structure used when determining what hardware the driver supports. */

static const struct pci_device_id vmxnet_chips[] =
   {
      {
         PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_NET),
         .driver_data = VMXNET_CHIP,
      },
      {
         PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE),
         .driver_data = LANCE_CHIP,
      },
      {
         0,
      },
   };

static struct pci_driver vmxnet_driver = {
					    .name = "vmxnet",
                                            .id_table = vmxnet_chips,
                                            .probe = vmxnet_probe_device,
                                            .remove = vmxnet_remove_device,
                                         };

#ifdef ETHTOOL_GSET
/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_get_settings --
 *
 *    Ethtool handler to get device settings.
 *
 *  Results:
 *    0 if successful, error code otherwise. Settings are copied to addr.
 *
 *  Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_get_settings(struct net_device *dev, void *addr)
{
   struct ethtool_cmd cmd;
   memset(&cmd, 0, sizeof(cmd));
   cmd.speed = 1000;     // 1 Gb
   cmd.duplex = 1;       // full-duplex
   cmd.maxtxpkt = 1;     // no tx coalescing
   cmd.maxrxpkt = 1;     // no rx coalescing
   cmd.autoneg = 0;      // no autoneg
   cmd.advertising = 0;  // advertise nothing

   return copy_to_user(addr, &cmd, sizeof(cmd));
}
#endif


#ifdef ETHTOOL_GLINK
/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_get_link --
 *
 *    Ethtool handler to get the link state.
 *
 *  Results:
 *    0 if successful, error code otherwise. The link status is copied to
 *    addr.
 *
 *  Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_get_link(struct net_device *dev, void *addr)
{
   struct ethtool_value value = {ETHTOOL_GLINK};
   value.data = netif_carrier_ok(dev) ? 1 : 0;
   return copy_to_user(addr, &value, sizeof(value));
}
#endif


#ifdef SIOCETHTOOL
/*
 *----------------------------------------------------------------------------
 *
 *  vmxnet_ethtool_ioctl --
 * 
 *    Handler for ethtool ioctl calls.
 *
 *  Results:
 *    If ethtool op is supported, the outcome of the op. Otherwise,
 *    -EOPNOTSUPP.
 *
 *  Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_ethtool_ioctl(struct net_device *dev, struct ifreq *ifr)
{
   uint32_t cmd;
   if (copy_from_user(&cmd, ifr->ifr_data, sizeof(cmd))) {
      return -EFAULT;
   }
   switch (cmd) {
#ifdef ETHTOOL_GSET
      case ETHTOOL_GSET:
         return vmxnet_get_settings(dev, ifr->ifr_data);
#endif
#ifdef ETHTOOL_GLINK
      case ETHTOOL_GLINK:
         return vmxnet_get_link(dev, ifr->ifr_data);
#endif
      default:
         printk(KERN_DEBUG" ethtool operation %d not supported\n", cmd);
         return -EOPNOTSUPP;
   }
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * vmxnet_ioctl --
 *
 *    Handler for ioctl calls.
 *
 * Results:
 *    If ioctl is supported, the result of that operation. Otherwise,
 *    -EOPNOTSUPP.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
vmxnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   switch (cmd) {
#ifdef SIOCETHTOOL
      case SIOCETHTOOL:
         return vmxnet_ethtool_ioctl(dev, ifr);
#endif
   }
   printk(KERN_DEBUG" ioctl operation %d not supported\n", cmd);
   return -EOPNOTSUPP;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_init --
 *
 *      Initialization, called by Linux when the module is loaded.
 *
 * Results:
 *      Returns 0 for success, negative errno value otherwise.
 *
 * Side effects:
 *      See vmxnet_probe_device, which does all the work.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet_init(void)
{
   int err;

   if (vmxnet_debug > 0) {
      vmxnet_debug = debug;
   }

   printk(KERN_INFO "VMware vmxnet virtual NIC driver\n");

   err = pci_register_driver(&vmxnet_driver);
   if (err < 0) {
      return err;
   }
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_exit --
 *
 *      Cleanup, called by Linux when the module is unloaded.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters all vmxnet devices with Linux and frees memory.
 *
 *-----------------------------------------------------------------------------
 */

static void
vmxnet_exit(void)
{
   pci_unregister_driver(&vmxnet_driver);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_tx_timeout --
 *
 *      Network device tx_timeout routine.  Called by Linux when the tx
 *      queue has been stopped for more than dev->watchdog_timeo jiffies.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Tries to restart the transmit queue.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_tx_timeout(struct net_device *dev)
{
   netif_wake_queue(dev);
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43) */


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_probe_device --
 *
 *      Most of the initialization at module load time is done here.
 *
 * Results:
 *      Returns 0 for success, an error otherwise.
 *
 * Side effects:
 *      Switches device from vlance to vmxnet mode, creates ethernet
 *      structure for device, and registers device with network stack.
 *
 *-----------------------------------------------------------------------------
 */

static int
vmxnet_probe_device(struct pci_dev             *pdev, // IN: vmxnet PCI device
                    const struct pci_device_id *id)   // IN: matching device ID
{
   struct Vmxnet_Private *lp;
   struct net_device *dev;
   unsigned int ioaddr, reqIOAddr, reqIOSize;
   unsigned int irq_line;
   /* VMware's version of the magic number */
   unsigned int low_vmware_version;
   unsigned int numRxBuffers;
   unsigned int numTxBuffers;
   Bool morphed = FALSE;
   int i;
   unsigned int driverDataSize;

   i = compat_pci_enable_device(pdev);
   if (i) {
      printk(KERN_ERR "Cannot enable vmxnet adapter %s: error %d\n",
             compat_pci_name(pdev), i);
      return i;
   }
   compat_pci_set_master(pdev);
   irq_line = pdev->irq;
   ioaddr = compat_pci_resource_start(pdev, 0);

   reqIOAddr = ioaddr;
   /* Found adapter, adjust ioaddr to match the adapter we found. */
   if (id->driver_data == VMXNET_CHIP) {
      reqIOSize = VMXNET_CHIP_IO_RESV_SIZE;
   } else {
      /*
       * Since this is a vlance adapter we can only use it if
       * its I/0 space is big enough for the adapter to be
       * capable of morphing. This is the first requirement
       * for this adapter to potentially be morphable. The
       * layout of a morphable LANCE adapter is
       *
       * I/O space:
       *
       * |------------------|
       * | LANCE IO PORTS   |
       * |------------------|
       * | MORPH PORT       |
       * |------------------|
       * | VMXNET IO PORTS  |
       * |------------------|
       *
       * VLance has 8 ports of size 4 bytes, the morph port is 4 bytes, and
       * Vmxnet has 10 ports of size 4 bytes.
       *
       * We shift up the ioaddr with the size of the LANCE I/O space since
       * we want to access the vmxnet ports. We also shift the ioaddr up by
       * the MORPH_PORT_SIZE so other port access can be independent of
       * whether we are Vmxnet or a morphed VLance. This means that when
       * we want to access the MORPH port we need to subtract the size
       * from ioaddr to get to it.
       */

      ioaddr += LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE;
      reqIOSize = LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE +
                  VMXNET_CHIP_IO_RESV_SIZE;
   }
   /* Do not attempt to morph non-morphable AMD PCnet */
   if (reqIOSize > compat_pci_resource_len(pdev, 0)) {
      printk(KERN_INFO "vmxnet: Device in slot %s is not supported by this driver.\n",
             compat_pci_name(pdev));
      goto pci_disable;
   }

   /*
    * Request I/O region with adjusted base address and size. The adjusted
    * values are needed and used if we release the region in case of failure.
    */

   if (!compat_request_region(reqIOAddr, reqIOSize, VMXNET_CHIP_NAME)) {
      printk(KERN_INFO "vmxnet: Another driver already loaded for device in slot %s.\n",
             compat_pci_name(pdev));
      goto pci_disable;
   }

   /* Morph the underlying hardware if we found a VLance adapter. */
   if (id->driver_data == LANCE_CHIP) {
      uint16 magic;

      /* Read morph port to verify that we can morph the adapter. */

      magic = inw(ioaddr - MORPH_PORT_SIZE);
      if (magic != LANCE_CHIP &&
          magic != VMXNET_CHIP) {
         printk(KERN_ERR "Invalid magic, read: 0x%08X\n", magic);
         goto release_reg;
      }

      /* Morph adapter. */

      outw(VMXNET_CHIP, ioaddr - MORPH_PORT_SIZE);
      morphed = TRUE;

      /* Verify that we morphed correctly. */

      magic = inw(ioaddr - MORPH_PORT_SIZE);
      if (magic != VMXNET_CHIP) {
         printk(KERN_ERR "Couldn't morph adapter. Invalid magic, read: 0x%08X\n",
                magic);
         goto morph_back;
      }
   }

   printk(KERN_INFO "Found vmxnet/PCI at %#x, irq %u.\n", ioaddr, irq_line);

   low_vmware_version = inl(ioaddr + VMXNET_LOW_VERSION);
   if ((low_vmware_version & 0xffff0000) != (VMXNET2_MAGIC & 0xffff0000)) {
      printk(KERN_ERR "Driver version 0x%08X doesn't match version 0x%08X\n",
             VMXNET2_MAGIC, low_vmware_version);
      goto morph_back;
   } else {
      /*
       * The low version looked OK so get the high version and make sure that
       * our version is supported.
       */
      unsigned int high_vmware_version = inl(ioaddr + VMXNET_HIGH_VERSION);
      if ((VMXNET2_MAGIC < low_vmware_version) ||
          (VMXNET2_MAGIC > high_vmware_version)) {
         printk(KERN_ERR
                "Driver version 0x%08X doesn't match version 0x%08X, 0x%08X\n",
                VMXNET2_MAGIC, low_vmware_version, high_vmware_version);
         goto morph_back;
      }
   }

   dev = compat_alloc_etherdev(sizeof *lp);
   if (!dev) {
      printk(KERN_ERR "Unable to allocate ethernet device\n");
      goto morph_back;
   }

   dev->base_addr = ioaddr;

   outl(VMXNET_CMD_GET_NUM_RX_BUFFERS, dev->base_addr + VMXNET_COMMAND_ADDR);
   numRxBuffers = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   if (numRxBuffers == 0 || numRxBuffers > VMXNET2_MAX_NUM_RX_BUFFERS) {
      numRxBuffers = VMXNET2_DEFAULT_NUM_RX_BUFFERS;
   }

   outl(VMXNET_CMD_GET_NUM_TX_BUFFERS, dev->base_addr + VMXNET_COMMAND_ADDR);
   numTxBuffers = inl(dev->base_addr + VMXNET_COMMAND_ADDR);
   if (numTxBuffers == 0 || numTxBuffers > VMXNET2_MAX_NUM_TX_BUFFERS) {
      numTxBuffers = VMXNET2_DEFAULT_NUM_TX_BUFFERS;
   }

   driverDataSize =
            sizeof(Vmxnet2_DriverData) +
            (numRxBuffers + 1) * sizeof(Vmxnet2_RxRingEntry) + /* +1 for dummy rxRing2 */
            numTxBuffers * sizeof(Vmxnet2_TxRingEntry);
   VMXNET_LOG("vmxnet: numRxBuffers=(%d*%d) numTxBuffers=(%d*%d) driverDataSize=%d\n",
              numRxBuffers, (uint32)sizeof(Vmxnet2_RxRingEntry),
              numTxBuffers, (uint32)sizeof(Vmxnet2_TxRingEntry),
              driverDataSize);
   lp = dev->priv;
   lp->pdev = pdev;
   lp->ddAllocated = kmalloc(driverDataSize + 15, GFP_DMA | GFP_KERNEL);

   if (!lp->ddAllocated) {
      printk(KERN_ERR "Unable to allocate memory for driver data\n");
      goto free_dev;
   }
   if ((uintptr_t)virt_to_bus(lp->ddAllocated) > SHARED_MEM_MAX) {
      printk(KERN_ERR
             "Unable to initialize driver data, address outside of shared area (0x%p)\n",
             (void*)virt_to_bus(lp->ddAllocated));
      goto free_dev_dd;
   }

   /* Align on paragraph boundary */
   lp->dd = (Vmxnet2_DriverData*)(((unsigned long)lp->ddAllocated + 15) & ~15UL);
   memset(lp->dd, 0, driverDataSize);
   spin_lock_init(&lp->txLock);
   lp->numRxBuffers = numRxBuffers;
   lp->numTxBuffers = numTxBuffers;
   /* So that the vmkernel can check it is compatible */
   lp->dd->magic = VMXNET2_MAGIC;
   lp->dd->length = driverDataSize;
   lp->name = VMXNET_CHIP_NAME;

   /*
    * Store whether we are morphed so we can figure out how to
    * clean up when we unload.
    */
   lp->morphed = morphed;

   outl(VMXNET_CMD_GET_FEATURES, dev->base_addr + VMXNET_COMMAND_ADDR);
   lp->features = inl(dev->base_addr + VMXNET_COMMAND_ADDR);

   outl(VMXNET_CMD_GET_CAPABILITIES, dev->base_addr + VMXNET_COMMAND_ADDR);
   lp->capabilities = inl(dev->base_addr + VMXNET_COMMAND_ADDR);

   if (lp->capabilities & VMNET_CAP_VMXNET_APROM) {
      for (i = 0; i < ETH_ALEN; i++) {
         dev->dev_addr[i] = inb(ioaddr + VMXNET_APROM_ADDR + i);
      }
      for (i = 0; i < ETH_ALEN; i++) {
         outb(dev->dev_addr[i], ioaddr + VMXNET_MAC_ADDR + i);
      }
   } else {
      /*
       * Be backwards compatible and use the MAC address register to
       * get MAC address.
       */
      for (i = 0; i < ETH_ALEN; i++) {
         dev->dev_addr[i] = inb(ioaddr + VMXNET_MAC_ADDR + i);
      }
   }
   lp->zeroCopyTx = FALSE;
   lp->partialHeaderCopyEnabled = FALSE;
   lp->tso = FALSE;

   printk(KERN_INFO "features:");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
   if (lp->capabilities & VMNET_CAP_IP4_CSUM) {
      dev->features |= NETIF_F_IP_CSUM;
      printk(" ipCsum");
   }
   if (lp->capabilities & VMNET_CAP_HW_CSUM) {
      dev->features |= NETIF_F_HW_CSUM;
      printk(" hwCsum");
   }
#endif

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->capabilities & VMNET_CAP_SG &&
       lp->features & VMXNET_FEATURE_ZERO_COPY_TX){
      dev->features |= NETIF_F_SG;
      lp->zeroCopyTx = TRUE;
      printk(" zeroCopy");

      if (lp->capabilities & VMNET_CAP_ENABLE_HEADER_COPY) {
         lp->partialHeaderCopyEnabled = TRUE;
         printk(" partialHeaderCopy");
      }
   }

   lp->txBufferStart = NULL;
   lp->dd->txBufferPhysStart = 0;
   lp->dd->txBufferPhysLength = 0;

   if (lp->partialHeaderCopyEnabled){
      unsigned int txBufferSize;
      
      txBufferSize = numTxBuffers * TX_PKT_HEADER_SIZE;
      lp->txBufferStartRaw = kmalloc(txBufferSize + PAGE_SIZE,
                                     GFP_DMA | GFP_KERNEL);
      if (lp->txBufferStartRaw) {
         lp->txBufferStart = (char*)((unsigned long)(lp->txBufferStartRaw + PAGE_SIZE - 1) & 
                                     (unsigned long)~(PAGE_SIZE - 1));
         lp->dd->txBufferPhysStart = virt_to_phys(lp->txBufferStart); 
         lp->dd->txBufferPhysLength = txBufferSize;
         lp->dd->txPktMaxSize = TX_PKT_HEADER_SIZE;
      } else {
         lp->partialHeaderCopyEnabled = FALSE;
      }
   }
#endif

#ifdef VMXNET_DO_TSO
   if ((lp->capabilities & VMNET_CAP_TSO) &&
       (lp->capabilities & (VMNET_CAP_IP4_CSUM | VMNET_CAP_HW_CSUM)) && 
       // tso only makes sense if we have hw csum offload
       lp->features & VMXNET_FEATURE_TSO){
      dev->features |= NETIF_F_TSO;
      lp->tso = TRUE;
      printk(" tso");
   }
#endif
   printk("\n");
  
   dev->irq = irq_line;

   dev->open = &vmxnet_open;
   dev->hard_start_xmit = &vmxnet_start_tx;
   dev->stop = &vmxnet_close;
   dev->get_stats = &vmxnet_get_stats;
   dev->set_multicast_list = &vmxnet_set_multicast_list;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,43)
   dev->tx_timeout = &vmxnet_tx_timeout;
   dev->watchdog_timeo = VMXNET_WATCHDOG_TIMEOUT;
#endif
#ifdef VMW_HAVE_POLL_CONTROLLER
   dev->poll_controller = vmxnet_netpoll;
#endif

   /* Do this after ether_setup(), which sets the default value. */
   dev->set_mac_address = &vmxnet_set_mac_address;
   dev->do_ioctl = vmxnet_ioctl;

   COMPAT_SET_MODULE_OWNER(dev);

   if (register_netdev(dev)) {
      printk(KERN_ERR "Unable to register device\n");
      goto free_dev_dd;
   }

   /* Do this after register_netdev(), which sets device name */
   VMXNET_LOG("%s: %s at %#3lx assigned IRQ %d.\n",
              dev->name, lp->name, dev->base_addr, dev->irq);

   pci_set_drvdata(pdev, dev);
   return 0;

free_dev_dd:;
   kfree(lp->ddAllocated);
free_dev:;
   compat_free_netdev(dev);
morph_back:;
   if (morphed) {
      /* Morph back to LANCE hw. */
      outw(LANCE_CHIP, ioaddr - MORPH_PORT_SIZE);
   }
release_reg:;
   release_region(reqIOAddr, reqIOSize);
pci_disable:;
   compat_pci_disable_device(pdev);
   return -EBUSY;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_remove_device --
 *
 *      Cleanup, called for each device on unload.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Unregisters vmxnet device with Linux and frees memory.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_remove_device(struct pci_dev* pdev)
{
   struct net_device *dev = pci_get_drvdata(pdev);
   struct Vmxnet_Private *lp = dev->priv;

   unregister_netdev(dev);

   /* Unmorph adapter if it was morphed. */

   if (lp->morphed) {
      uint16 magic;

      /* Read morph port to verify that we can morph the adapter. */

      magic = inw(dev->base_addr - MORPH_PORT_SIZE);
      if (magic != VMXNET_CHIP) {
         printk(KERN_ERR "Adapter not morphed. read magic: 0x%08X\n", magic);
      }

      /* Morph adapter back to LANCE. */

      outw(LANCE_CHIP, dev->base_addr - MORPH_PORT_SIZE);

      /* Verify that we unmorphed correctly. */

      magic = inw(dev->base_addr - MORPH_PORT_SIZE);
      if (magic != LANCE_CHIP) {
         printk(KERN_ERR "Couldn't unmorph adapter. Invalid magic, read: 0x%08X\n",
                magic);
      }

      release_region(dev->base_addr -
                     (LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE),
                     VMXNET_CHIP_IO_RESV_SIZE +
                     (LANCE_CHIP_IO_RESV_SIZE + MORPH_PORT_SIZE));
   } else {
      release_region(dev->base_addr, VMXNET_CHIP_IO_RESV_SIZE);
   }

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->partialHeaderCopyEnabled){
      kfree(lp->txBufferStartRaw);
   }
#endif

   kfree(lp->ddAllocated);
   compat_free_netdev(dev);
   compat_pci_disable_device(pdev);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_init_ring --
 *
 *      Initializes buffer rings in Vmxnet_Private structure.  Allocates skbs
 *      to receive into.  Called by vmxnet_open.
 *
 * Results:
 *      0 on success; -1 on failure to allocate skbs.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_init_ring(struct net_device *dev)
{
   struct Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;
   Vmxnet2_DriverData *dd = lp->dd;
   int i;
   size_t offset;

   offset = sizeof(*dd);

   dd->rxRingLength = lp->numRxBuffers;
   dd->rxRingOffset = offset;
   lp->rxRing = (Vmxnet2_RxRingEntry *)((uintptr_t)dd + offset);
   offset += lp->numRxBuffers * sizeof(Vmxnet2_RxRingEntry);
   
   // dummy rxRing2
   dd->rxRingLength2 = 1;
   dd->rxRingOffset2 = offset;
   offset += sizeof(Vmxnet2_RxRingEntry);

   dd->txRingLength = lp->numTxBuffers;
   dd->txRingOffset = offset;
   lp->txRing = (Vmxnet2_TxRingEntry *)((uintptr_t)dd + offset);
   offset += lp->numTxBuffers * sizeof(Vmxnet2_TxRingEntry);

   VMXNET_LOG("vmxnet_init_ring: offset=%"FMT64"d length=%d\n", 
              (uint64)offset, dd->length);

   for (i = 0; i < lp->numRxBuffers; i++) {
      lp->rxSkbuff[i] = dev_alloc_skb (PKT_BUF_SZ);
      if (lp->rxSkbuff[i] == NULL) {
	 // there is not much we can do at this point
	 printk (KERN_ERR "%s: vmxnet_init_ring dev_alloc_skb failed.\n", dev->name);
	 return -ENOMEM;
      }

      lp->rxRing[i].paddr = virt_to_bus(compat_skb_tail_pointer(lp->rxSkbuff[i]));
      lp->rxRing[i].bufferLength = PKT_BUF_SZ;
      lp->rxRing[i].actualLength = 0;
      lp->rxRingBuffPtr[i] = &lp->rxSkbuff[i];
      lp->rxRing[i].ownership = VMXNET2_OWNERSHIP_NIC;
   }
   
   // dummy rxRing2 tacked on to the end, with a single unusable entry
   lp->rxRing[i].paddr = 0;
   lp->rxRing[i].bufferLength = 0;
   lp->rxRing[i].actualLength = 0;
   lp->rxRingBuffPtr[i] = NULL;
   lp->rxRing[i].ownership = VMXNET2_OWNERSHIP_DRIVER;

   dd->rxDriverNext = 0;

   for (i = 0; i < lp->numTxBuffers; i++) {
      lp->txRing[i].ownership = VMXNET2_OWNERSHIP_DRIVER;
      lp->txBufInfo[i].skb = NULL;
      lp->txBufInfo[i].eop = 0;
      lp->txRing[i].sg.sg[0].addrHi = 0;
      lp->txRing[i].sg.addrType = NET_SG_PHYS_ADDR;
   }

   dd->txDriverCur = dd->txDriverNext = 0;
   dd->savedRxNICNext = dd->savedRxNICNext2 = dd->savedTxNICNext = 0;
   dd->txStopped = FALSE;

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_open --
 *
 *      Network device open routine.  Called by Linux when the interface is
 *      brought up.
 *
 * Results:
 *      0 on success; else negative errno value.
 *
 * Side effects:
 *      Allocates an IRQ if not already allocated.  Sets our Vmxnet_Private
 *      structure to be the shared area with the lower layer.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_open(struct net_device *dev)
{
   struct Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;
   unsigned int ioaddr = dev->base_addr;
   u32 paddr;

   if (dev->irq == 0 ||	request_irq(dev->irq, &vmxnet_interrupt,
			            COMPAT_IRQF_SHARED, lp->name, (void *)dev)) {
      return -EAGAIN;
   }

   if (vmxnet_debug > 1) {
      printk(KERN_DEBUG "%s: vmxnet_open() irq %d lp %#x.\n",
	     dev->name, dev->irq,
	     (u32) virt_to_bus(lp));
   }

   if (vmxnet_init_ring(dev)) {
      return -ENOMEM;
   }

   paddr = virt_to_bus(lp->dd);

   outl(paddr, ioaddr + VMXNET_INIT_ADDR);
   outl(lp->dd->length, ioaddr + VMXNET_INIT_LENGTH);

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->partialHeaderCopyEnabled) {
      outl(VMXNET_CMD_PIN_TX_BUFFERS, ioaddr + VMXNET_COMMAND_ADDR);
   }
   // Pin the Tx buffers if partial header copy is enabled
#endif

   lp->dd->txStopped = FALSE;
   netif_start_queue(dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->interrupt = 0;
   dev->start = 1;
#endif

   lp->devOpen = TRUE;

   COMPAT_NETDEV_MOD_INC_USE_COUNT;

   return 0;
}

#ifdef VMXNET_DO_ZERO_COPY
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_unmap_buf  --
 *
 *      Unmap the PAs that we pinned for DMA.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
vmxnet_unmap_buf(struct sk_buff *skb, 
                 struct Vmxnet_TxBuf *tb, 
                 Vmxnet2_TxRingEntry *xre, 
                 struct pci_dev *pdev)
{
   int sgIdx;

   // unmap the mapping for skb->data if needed
   if (tb->sgForLinear >= 0) {
      pci_unmap_single(pdev, 
                       VMXNET_GET_DMA_ADDR(xre->sg.sg[(int)tb->sgForLinear]), 
                       xre->sg.sg[(int)tb->sgForLinear].length, 
                       PCI_DMA_TODEVICE);
      VMXNET_LOG("vmxnet_unmap_buf: size %u\n", xre->sg.sg[tb->sgForLinear].length);
   } 
   
   // unmap the mapping for skb->frags[]
   for (sgIdx = tb->firstSgForFrag; sgIdx < xre->sg.length; sgIdx++) {
      pci_unmap_page(pdev, 
                     VMXNET_GET_DMA_ADDR(xre->sg.sg[sgIdx]), 
                     xre->sg.sg[sgIdx].length, 
                     PCI_DMA_TODEVICE);
      VMXNET_LOG("vmxnet_unmap_buf: sg %d Size %u\n", sgIdx, 
                      xre->sg.sg[sgIdx].length);
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_map_buf  --
 *
 *      Map the PAs that we need for DMA and populate the SG. tmc->offset
 *      indicates the position inside the pkt frame where mapping should start.
 *      
 *      xre->sg.length indicates # of sg entries already used. The mapping stops 
 *      when:
 *      1. all the data is mapped 
 *      2. we run out of sg entries.
 *      3. skb has a giant linear part, and this tx entry is to map the linear
 *         part
 *
 * Results:
 *      return TRUE when all the data is mapped and FALSE otherwise
 *
 * Side effects:
 *      tmc is updated. tb->sgIdx* and tb->eop are updated
 *
 *-----------------------------------------------------------------------------
 */

Bool
vmxnet_map_buf(struct sk_buff *skb, 
               struct Vmxnet2_TxRingEntry *xre,
               struct Vmxnet_TxBuf *tb,
               struct Vmxnet_TxMapContext *tmc, 
               struct pci_dev *pdev)
{
   int nextFrag = 0, nextSg = xre->sg.length;
   struct skb_frag_struct *frag;
   unsigned int offset = tmc->offset, len;
   dma_addr_t dma;

   tmc->mappedLen = 0;

   if (tmc->nextFrag == TMC_FRAG_LINEAR) {
      if (LIKELY(offset < skb_headlen(skb))) {
         len = skb_headlen(skb) - offset;
         if (UNLIKELY(len > VMXNET_MAX_LINEAR)) {
            len = VMXNET_MAX_LINEAR;
         }

         dma = pci_map_single(pdev, 
                              skb->data + offset, 
                              len,
                              PCI_DMA_TODEVICE);
         xre->sg.sg[nextSg].addrLow = VMXNET_GET_LO_ADDR(dma);
         xre->sg.sg[nextSg].addrHi = VMXNET_GET_HI_ADDR(dma);
         xre->sg.sg[nextSg].length = len;
         VMXNET_LOG("vmxnet_map_buf: offset %u size %u\n", offset, len);
         tmc->mappedLen += len;

         tb->sgForLinear = nextSg;
         nextSg++;
         tb->firstSgForFrag = nextSg;

         if (UNLIKELY(tmc->giantLinear)) {
            // prepare for the next tx entry
            tmc->offset += len;
            if (tmc->offset == skb_headlen(skb)) {
               tmc->nextFrag = 0; // linear part is done, move to the 1st frag
            }
            /* 
             * for a skb w/ a giant linear part, we dedicate some tx entries 
             * to the linear part, so don't try to map frags even if we have
             * enough sg
             */ 
            goto done;
         }
      } else {
         // this really should not happen
         tb->sgForLinear = -1;
         tb->firstSgForFrag = nextSg;
         offset -= skb_headlen(skb);

         // skip those frags that are completely copied 
         for ( ; nextFrag < skb_shinfo(skb)->nr_frags; nextFrag++){
            frag = &skb_shinfo(skb)->frags[nextFrag];
            
            if (offset >= frag->size){
               offset -= frag->size;
            } else {
               break;
            }
         }

         if (UNLIKELY(offset != 0)) {
            // map the remaining part of the frag that is not completely copied
            VMXNET_ASSERT(nextFrag < skb_shinfo(skb)->nr_frags);

            frag = &skb_shinfo(skb)->frags[nextFrag];
            dma = pci_map_page(pdev,
                               frag->page,
                               frag->page_offset,
                               frag->size,
                               PCI_DMA_TODEVICE);
            dma += offset;
            xre->sg.sg[nextSg].addrLow = VMXNET_GET_LO_ADDR(dma);
            xre->sg.sg[nextSg].addrHi = VMXNET_GET_HI_ADDR(dma);
            xre->sg.sg[nextSg].length = frag->size - offset;
            tmc->mappedLen += xre->sg.sg[nextSg].length;
            nextSg++;
            nextFrag++;
         }
      }
   } else {
      nextFrag = tmc->nextFrag;
      VMXNET_ASSERT(nextFrag >= 0 && nextFrag < skb_shinfo(skb)->nr_frags);
      tb->sgForLinear = -1;
      tb->firstSgForFrag = nextSg;
   }

   // map the remaining frags until sg entries are used
   for ( ; 
        nextFrag < skb_shinfo(skb)->nr_frags && nextSg < VMXNET2_SG_DEFAULT_LENGTH; 
        nextFrag++, nextSg++) {
      frag = &skb_shinfo(skb)->frags[nextFrag];
      dma = pci_map_page(pdev, 
                         frag->page,
                         frag->page_offset,
                         frag->size,
                         PCI_DMA_TODEVICE);
      xre->sg.sg[nextSg].addrLow = VMXNET_GET_LO_ADDR(dma);
      xre->sg.sg[nextSg].addrHi = VMXNET_GET_HI_ADDR(dma);
      xre->sg.sg[nextSg].length = frag->size;
      VMXNET_LOG("vmxnet_map_buf: sg %d size %u\n", nextSg, frag->size);
      tmc->mappedLen += frag->size;
   }
   tmc->nextFrag = nextFrag;

done:
   xre->sg.length = nextSg;
   VMXNET_ASSERT(xre->sg.length <= VMXNET2_SG_DEFAULT_LENGTH);
   tb->eop = (tmc->nextFrag >= skb_shinfo(skb)->nr_frags);

   VMXNET_LOG("vmxnet_map_buf: %dB mapped, %d sg used, nextFrag %d, eop %d\n",
              tmc->mappedLen, nextSg, nextFrag, tb->eop);

   return tb->eop;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * check_tx_queue --
 *
 *      Loop through the tx ring looking for completed transmits.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static void
check_tx_queue(struct net_device *dev)
{
   Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;
   Vmxnet2_DriverData *dd = lp->dd;
   int completed = 0;

   while (1) {
      Vmxnet2_TxRingEntry *xre = &lp->txRing[dd->txDriverCur];
      struct sk_buff *skb = lp->txBufInfo[dd->txDriverCur].skb;

      if (xre->ownership != VMXNET2_OWNERSHIP_DRIVER || skb == NULL) {
	 break;
      }
#ifdef VMXNET_DO_ZERO_COPY
      if (lp->zeroCopyTx){
         vmxnet_unmap_buf(skb, &lp->txBufInfo[dd->txDriverCur], xre, lp->pdev);
      }
#endif

      if (lp->txBufInfo[dd->txDriverCur].eop) {
         compat_dev_kfree_skb_irq(skb, FREE_WRITE);
      }
      lp->txBufInfo[dd->txDriverCur].skb = NULL;

      completed ++;

      VMXNET_INC(dd->txDriverCur, dd->txRingLength);
   }

   if (completed){
      lp->numTxPending -= completed;

      // XXX conditionally wake up the queue based on the # of freed entries
      if (netif_queue_stopped(dev)) {
	 netif_wake_queue(dev);
         dd->txStopped = FALSE;
      }
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_tx --
 *
 *      Network device hard_start_xmit helper routine.  This is called by
 *	the drivers hard_start_xmit routine when it wants to send a packet.
 *
 * Results:
 *      VMXNET_CALL_TRANSMIT:	The driver should ask the virtual NIC to
 *				transmit a packet.
 *      VMXNET_DEFER_TRANSMIT:	This transmit is deferred because of
 *				transmit clustering.
 *      VMXNET_STOP_TRANSMIT:	We ran out of queue space so the caller
 *				should stop transmitting.
 *
 * Side effects:
 *	The drivers tx ring may get modified.
 *
 *-----------------------------------------------------------------------------
 */
Vmxnet_TxStatus
vmxnet_tx(struct sk_buff *skb, struct net_device *dev)
{
   Vmxnet_TxStatus status = VMXNET_DEFER_TRANSMIT;
   struct Vmxnet_Private *lp = (struct Vmxnet_Private *)dev->priv;
   Vmxnet2_DriverData *dd = lp->dd;
   unsigned long flags;
   Vmxnet2_TxRingEntry *xre;

#ifdef VMXNET_DO_ZERO_COPY
   if (lp->zeroCopyTx) {
      int txEntries;
      int headerSize, i, linearPayload;
      unsigned int idx, iphOffset, tcphOffset;
      struct Vmxnet_TxMapContext mapContext;

      /* figure out the protocol and header size */
      iphOffset = ETHERNET_HEADER_SIZE;
      if (UNLIKELY(*(uint16*)(skb->data + ETH_FRAME_TYPE_LOCATION) == ETH_TYPE_VLAN_TAG)) {
         iphOffset += VLAN_TAG_LENGTH;
      }
      if (LIKELY(PKT_OF_IPV4(skb))){
         if (LIKELY(compat_skb_ip_header(skb)->protocol == IPPROTO_TCP)) {
            /* tcp traffic, copy all protocol headers */
            tcphOffset = iphOffset + (compat_skb_ip_header(skb)->ihl << 2);
            headerSize = tcphOffset + (compat_skb_tcp_header(skb)->doff << 2);
         } else {
            /* non-tcp traffic, copy mac + ip header */
            headerSize = iphOffset + (compat_skb_ip_header(skb)->ihl << 2);
            tcphOffset = 0;
         }
      } else { 
         /* non-ip traffic, only copy the mac header */
         headerSize = iphOffset;
         tcphOffset = 0;
      }
    
      /* 
       * decide the # of tx entries needed in the worse case 
       * NB: each frag is guaranteed to be at most one page
       */
      linearPayload = skb_headlen(skb) - headerSize;
      mapContext.giantLinear = (linearPayload > VMXNET_MAX_LINEAR);
      if (UNLIKELY(mapContext.giantLinear)) {
         /* tx entries dedicated to the linear part */
         txEntries = (linearPayload + VMXNET_MAX_LINEAR - 1) / VMXNET_MAX_LINEAR;
         /* 
          * since we need to add protocol headers for all tx entries,
          * only VMXNET2_SG_DEFAULT_LENGTH-1 sg entries are available
          */ 
         txEntries += (skb_shinfo(skb)->nr_frags + VMXNET2_SG_DEFAULT_LENGTH - 2) / 
                      (VMXNET2_SG_DEFAULT_LENGTH - 1);
      } else {
         int sgEntries;

         if (LIKELY(lp->partialHeaderCopyEnabled)) {
            sgEntries = skb_shinfo(skb)->nr_frags + 1 + (linearPayload > 0);
         } else {
            sgEntries = skb_shinfo(skb)->nr_frags + 1;
         }
         if (sgEntries <= VMXNET2_SG_DEFAULT_LENGTH) {
            txEntries = 1;
         } else {
            txEntries = ((sgEntries - VMXNET2_SG_DEFAULT_LENGTH) + VMXNET2_SG_DEFAULT_LENGTH - 2) / 
                        (VMXNET2_SG_DEFAULT_LENGTH - 1) + 1;
         }
      }
      VMXNET_LOG("\n%d(%d) bytes, %d tx entries\n", skb->len, skb_headlen(skb), txEntries);

      spin_lock_irqsave(&lp->txLock, flags);

      /* check for the availability of tx ring entries */
      for (i = 0, idx = dd->txDriverNext; i < txEntries; i++) {
         if (lp->txBufInfo[idx].skb != NULL) {
            dd->txStopped = TRUE;            
            netif_stop_queue(dev);
            check_tx_queue(dev);

            spin_unlock_irqrestore(&lp->txLock, flags);
            VMXNET_LOG("queue stopped\n");
            return VMXNET_STOP_TRANSMIT;
         }
         VMXNET_INC(idx, dd->txRingLength);
      }
     
      if (txEntries == 1) {
         dd->stats.noCopyTransmits++;
one_tx_entry:
         if (UNLIKELY(headerSize > dd->txPktMaxSize)) {
            headerSize = dd->txPktMaxSize;
         }

         xre = &lp->txRing[dd->txDriverNext];
         if (LIKELY(skb->ip_summed == VM_CHECKSUM_PARTIAL)) {
            xre->flags = VMXNET2_TX_HW_XSUM | VMXNET2_TX_CAN_KEEP;
         } else {
            xre->flags = VMXNET2_TX_CAN_KEEP;	 
         }
#ifdef VMXNET_DO_TSO
         if (skb_shinfo(skb)->tso_size){
            xre->flags |= VMXNET2_TX_TSO;
            xre->tsoMss = skb_shinfo(skb)->tso_size;
         }
#endif
         if (++lp->numTxPending > dd->txRingLength - 5) {
            xre->flags |= VMXNET2_TX_RING_LOW;
            status = VMXNET_CALL_TRANSMIT;
         }

         mapContext.nextFrag = TMC_FRAG_LINEAR;

         if (LIKELY(lp->partialHeaderCopyEnabled)) {
            unsigned int pos = dd->txDriverNext * dd->txPktMaxSize;
            char *header;
            
            header = lp->txBufferStart + pos;

            skb_copy_bits(skb, 0, header, headerSize);

            xre->sg.sg[0].addrLow = (uint32)dd->txBufferPhysStart + pos;
            xre->sg.sg[0].addrHi = 0;
            xre->sg.sg[0].length = headerSize;
            xre->sg.length = 1;
            mapContext.offset = headerSize;
         } else {
            xre->sg.length = 0;
            mapContext.offset = 0;
         }

         VMXNET_ASSERT(lp->txBufInfo[dd->txDriverNext].skb == NULL);

         lp->txBufInfo[dd->txDriverNext].skb = skb;
         vmxnet_map_buf(skb, xre, &lp->txBufInfo[dd->txDriverNext], 
                        &mapContext, lp->pdev);
         VMXNET_ASSERT(lp->txBufInfo[dd->txDriverNext].eop);

         wmb();
         xre->ownership = VMXNET2_OWNERSHIP_NIC;
         VMXNET_INC(dd->txDriverNext, dd->txRingLength);
      } else {
         u32 nextSeq;
         u16 nextId;
         unsigned int tcphSize, tcpiphSize;

         /* 
          * we don't split a large pkt when:
          * 1. it's not a tcp pkt
          * 2. partial header copy is disabled
          * 3. its header is too large 
          */
         if (UNLIKELY((tcphOffset == 0 || 
                       !lp->partialHeaderCopyEnabled || 
                       headerSize > dd->txPktMaxSize))) {
            spin_unlock_irqrestore(&lp->txLock, flags);

            if (compat_skb_linearize(skb) != 0) {
               VMXNET_LOG("vmxnet_tx: skb_linearize failed\n");
               dev_kfree_skb_any(skb);
               return VMXNET_DEFER_TRANSMIT;
            }
            
            spin_lock_irqsave(&lp->txLock, flags);
            dd->stats.copyTransmits++;
            goto one_tx_entry;
         }

         dd->stats.noCopyTransmits++;

         /* prepare for the chopping */
         nextSeq = ntohl(compat_skb_tcp_header(skb)->seq);
         nextId = ntohs(compat_skb_ip_header(skb)->id);
         tcphSize = headerSize - tcphOffset;
         tcpiphSize = headerSize - iphOffset;

         txEntries = 0;
         mapContext.nextFrag = TMC_FRAG_LINEAR;
         mapContext.offset = headerSize;

         /* let's chop it */
         while (1) {
            u32 csum;
            u16 oldId, oldLen;
            unsigned int pos;
            char *header;
            struct iphdr *iph;
            struct tcphdr *tcph;
            Bool eop;

            pos = dd->txDriverNext * dd->txPktMaxSize;
            header = lp->txBufferStart + pos;

            iph = (struct iphdr*)(header + iphOffset);
            tcph = (struct tcphdr *)(header + tcphOffset);

            // copy the protocol headers
            skb_copy_bits(skb, 0, header, headerSize);
            tcph->fin = 0;
            tcph->psh = 0;

            xre = &lp->txRing[dd->txDriverNext];
            xre->sg.sg[0].addrLow = (uint32)(dd->txBufferPhysStart + pos);
            xre->sg.sg[0].addrHi = 0;
            xre->sg.sg[0].length = headerSize;
            xre->sg.length = 1;
            
            VMXNET_ASSERT(lp->txBufInfo[dd->txDriverNext].skb == NULL);
            lp->txBufInfo[dd->txDriverNext].skb = skb;
            eop = vmxnet_map_buf(skb, xre, &lp->txBufInfo[dd->txDriverNext],
                                 &mapContext, lp->pdev);
             
            // update tcp/ip header and csum
            oldId = iph->id;
            iph->id = htons(nextId);
            nextId++;
            oldLen = iph->tot_len;
            iph->tot_len = htons(mapContext.mappedLen + tcpiphSize);
            csum = (u16)(~iph->check);
            csum += (u16)(~oldId) + iph->id + (u16)(~oldLen) + iph->tot_len;
            iph->check = csum_fold(csum);

            tcph->seq = htonl(nextSeq);
            nextSeq += mapContext.mappedLen;
            tcph->check = ~csum_tcpudp_magic(iph->saddr, 
                                             iph->daddr, 
                                             mapContext.mappedLen + tcphSize, 
                                             IPPROTO_TCP, 
                                             0);

            /* yes, we always ask for csum offload */
            xre->flags = VMXNET2_TX_HW_XSUM | VMXNET2_TX_CAN_KEEP;
#ifdef VMXNET_DO_TSO
            if (skb_shinfo(skb)->tso_size && mapContext.mappedLen > skb_shinfo(skb)->tso_size){
               xre->flags |= VMXNET2_TX_TSO;
               xre->tsoMss = skb_shinfo(skb)->tso_size;
            }
#endif
            VMXNET_LOG("entry %d, id %d, seq %u (+%d)\n", txEntries, htons(iph->id), 
                       htonl(tcph->seq), mapContext.mappedLen);

            txEntries++;

            if (!eop) {
               wmb();
               xre->ownership = VMXNET2_OWNERSHIP_NIC;
               VMXNET_INC(dd->txDriverNext, dd->txRingLength);
            } else {
               /* special considerations for the last one */
               tcph->fin = compat_skb_tcp_header(skb)->fin;
               tcph->psh = compat_skb_tcp_header(skb)->psh;

               lp->numTxPending += txEntries;
               if (lp->numTxPending > dd->txRingLength - 5) {
                  xre->flags |= VMXNET2_TX_RING_LOW;
                  status = VMXNET_CALL_TRANSMIT;
               }

               wmb();
               xre->ownership = VMXNET2_OWNERSHIP_NIC;
               VMXNET_INC(dd->txDriverNext, dd->txRingLength);

               break;
            }
         } // while (1)
      }
#ifdef VMXNET_DO_TSO
      if (skb_shinfo(skb)->tso_size){
         dd->txNumDeferred += ((skb->len - headerSize) + skb_shinfo(skb)->tso_size - 1) / 
                              skb_shinfo(skb)->tso_size;
      } else
#endif
      {
         dd->txNumDeferred++;
      }
   } else /* zero copy not enabled */
#endif
   {
      spin_lock_irqsave(&lp->txLock, flags);

      if (lp->txBufInfo[dd->txDriverNext].skb != NULL) {
         dd->txStopped = TRUE;
         netif_stop_queue(dev);
         check_tx_queue(dev);

         spin_unlock_irqrestore(&lp->txLock, flags);
         return VMXNET_STOP_TRANSMIT;
      }
     
      xre = &lp->txRing[dd->txDriverNext];
      if (skb->ip_summed == VM_CHECKSUM_PARTIAL) {
         xre->flags = VMXNET2_TX_HW_XSUM | VMXNET2_TX_CAN_KEEP;
      } else {
         xre->flags = VMXNET2_TX_CAN_KEEP;	 
      }
      lp->numTxPending ++;
      if (lp->numTxPending > dd->txRingLength - 5) {
         xre->flags |= VMXNET2_TX_RING_LOW;
         status = VMXNET_CALL_TRANSMIT;
      }

      xre->sg.length = 1;
      xre->sg.sg[0].length = skb->len;
      xre->sg.sg[0].addrLow = virt_to_bus(skb->data);
      xre->sg.sg[0].addrHi = 0;
      dd->stats.copyTransmits++;

      lp->txBufInfo[dd->txDriverNext].skb = skb;
      lp->txBufInfo[dd->txDriverNext].sgForLinear = 0;
      lp->txBufInfo[dd->txDriverNext].firstSgForFrag = -1;
      lp->txBufInfo[dd->txDriverNext].eop = 1;
      wmb();
      xre->ownership = VMXNET2_OWNERSHIP_NIC;

      VMXNET_INC(dd->txDriverNext, dd->txRingLength);
      dd->txNumDeferred++;
   }

   // common post-mapping work, xre is the last filled tx ring entry
   if (dd->txNumDeferred >= dd->txClusterLength) {
      dd->txNumDeferred = 0;
      status = VMXNET_CALL_TRANSMIT;
   }

   dev->trans_start = jiffies;

   lp->stats.tx_packets++;
   dd->stats.pktsTransmitted++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
   lp->stats.tx_bytes += skb->len;
#endif

   if (lp->numTxPending > dd->stats.maxTxsPending) {
      dd->stats.maxTxsPending = lp->numTxPending;
   }

   check_tx_queue(dev);

   spin_unlock_irqrestore(&lp->txLock, flags);

   return status;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_start_tx --
 *
 *      Network device hard_start_xmit routine.  Called by Linux when it has
 *      a packet for us to transmit.
 *
 * Results:
 *      0 on success; 1 if no resources.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_start_tx(struct sk_buff *skb, struct net_device *dev)
{
   int retVal = 0;
   Vmxnet_TxStatus xs = vmxnet_tx(skb, dev);
   switch (xs) {
   case VMXNET_CALL_TRANSMIT:
      inl(dev->base_addr + VMXNET_TX_ADDR);
      break;
   case VMXNET_DEFER_TRANSMIT:
      break;
   case VMXNET_STOP_TRANSMIT:
      retVal = 1;
      break;
   }

   return retVal;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_rx --
 *
 *      Receive a packet.
 *
 * Results:
 *      0
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_rx(struct net_device *dev)
{
   Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;
   Vmxnet2_DriverData *dd = lp->dd;

   if (!lp->devOpen) {
      return 0;
   }

   while (1) {
      short pkt_len;
      struct sk_buff *skb;
      Vmxnet2_RxRingEntry *rre;
      rre = &lp->rxRing[dd->rxDriverNext];
      if (rre->ownership != VMXNET2_OWNERSHIP_DRIVER) {
	 break;
      }
      pkt_len = rre->actualLength;

      if (pkt_len < (ETH_MIN_FRAME_LEN - 4)) {
         /*
          * Ethernet header vlan tags are 4 bytes.  Some vendors generate
          *  ETH_MIN_FRAME_LEN frames including vlan tags.  When vlan tag
          *  is stripped, such frames become ETH_MIN_FRAME_LEN - 4. (PR106153)
          */
         if (pkt_len != 0) {
	    printk(KERN_DEBUG "%s: Runt pkt (%d bytes)!\n", dev->name, pkt_len);
         }
	 lp->stats.rx_errors++;
      } else {
	 struct sk_buff *newskb = dev_alloc_skb (PKT_BUF_SZ);
	 if (newskb != NULL) {
	    struct sk_buff **rxSkbuff = lp->rxRingBuffPtr[dd->rxDriverNext];
	    skb = *rxSkbuff;
	    skb_put (skb, pkt_len);

	    newskb->dev = dev;
	    *rxSkbuff = newskb;	    
	    rre->paddr = virt_to_bus(newskb->tail);
	 } else {
	    skb = NULL;
	 }

	 if (skb == NULL) {
	    printk(KERN_DEBUG "%s: Memory squeeze, dropping packet.\n", dev->name);
	 } else {
            if (rre->flags & VMXNET2_RX_HW_XSUM_OK) {
               /* Cannot do anything better without having complete packet checksum. */
               skb->ip_summed = VM_CHECKSUM_UNNECESSARY;
            }
	    skb->dev = dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	    lp->stats.rx_bytes += skb->len;
#endif
	    skb->protocol = eth_type_trans(skb, dev);
	    netif_rx(skb);
	    lp->stats.rx_packets++;
            dd->stats.pktsReceived++;
	 }
      }

      rre->ownership = VMXNET2_OWNERSHIP_NIC;
      VMXNET_INC(dd->rxDriverNext, dd->rxRingLength);
   }

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_interrupt --
 *
 *      Interrupt handler.  Calls vmxnet_rx to receive a packet.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static compat_irqreturn_t
vmxnet_interrupt(int irq, void *dev_id, struct pt_regs * regs)
#else
static compat_irqreturn_t
vmxnet_interrupt(int irq, void *dev_id)
#endif
{
   struct net_device *dev = (struct net_device *)dev_id;
   struct Vmxnet_Private *lp;
   Vmxnet2_DriverData *dd;

   if (dev == NULL) {
      printk (KERN_DEBUG "vmxnet_interrupt(): irq %d for unknown device.\n", irq);
      return COMPAT_IRQ_NONE;
   }


   lp = (struct Vmxnet_Private *)dev->priv;
   outl(VMXNET_CMD_INTR_ACK, dev->base_addr + VMXNET_COMMAND_ADDR);

   dd = lp->dd;
   dd->stats.interrupts++;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   if (dev->interrupt) {
      printk(KERN_DEBUG "%s: Re-entering the interrupt handler.\n", dev->name);
   }
   dev->interrupt = 1;
#endif

   vmxnet_rx(dev);

   if (lp->numTxPending > 0) {
      spin_lock(&lp->txLock);
      check_tx_queue(dev);
      spin_unlock(&lp->txLock);
   }

   if (netif_queue_stopped(dev) && !lp->dd->txStopped) {
      netif_wake_queue(dev);
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->interrupt = 0;
#endif
   return COMPAT_IRQ_HANDLED;
}


#ifdef VMW_HAVE_POLL_CONTROLLER
/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_netpoll --
 *
 *      Poll network controller.  We reuse hardware interrupt for this.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Packets received/transmitted/whatever.
 *
 *-----------------------------------------------------------------------------
 */
static void
vmxnet_netpoll(struct net_device *dev)
{
   disable_irq(dev->irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
   vmxnet_interrupt(dev->irq, dev, NULL);
#else
   vmxnet_interrupt(dev->irq, dev);
#endif
   enable_irq(dev->irq);
}
#endif /* VMW_HAVE_POLL_CONTROLLER */


/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_close --
 *
 *      Network device stop (close) routine.  Called by Linux when the
 *      interface is brought down.
 *
 * Results:
 *      0 for success (always).
 *
 * Side effects:
 *      Flushes pending transmits.  Frees IRQs and shared memory area.
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_close(struct net_device *dev)
{
   unsigned int ioaddr = dev->base_addr;
   Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;
   int i;
   unsigned long flags;

   if (vmxnet_debug > 1) {
      printk(KERN_DEBUG "%s: Shutting down ethercard\n", dev->name);
   }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,43)
   dev->start = 0;
#endif

   netif_stop_queue(dev);

   lp->devOpen = FALSE;

   spin_lock_irqsave(&lp->txLock, flags);
   if (lp->numTxPending > 0) {
      //Wait absurdly long (2sec) for all the pending packets to be returned.
      printk(KERN_DEBUG "vmxnet_close: Pending tx = %d\n", lp->numTxPending); 
      for (i = 0; i < 200 && lp->numTxPending > 0; i++) {
	 outl(VMXNET_CMD_CHECK_TX_DONE, dev->base_addr + VMXNET_COMMAND_ADDR);
	 udelay(10000);
	 check_tx_queue(dev);
      }

      //This is possiblly caused by a faulty physical driver. 
      //Will go ahead and free these skb's anyways (possibly dangerous,
      //but seems to work in practice)
      if (lp->numTxPending > 0) {
         printk(KERN_EMERG "vmxnet_close: failed to finish all pending tx.\n"
                "This virtual machine may be in an inconsistent state.\n");
         lp->numTxPending = 0;
      }
   }
   spin_unlock_irqrestore(&lp->txLock, flags);
   
   outl(0, ioaddr + VMXNET_INIT_ADDR);

   free_irq(dev->irq, dev);

   for (i = 0; i < lp->dd->txRingLength; i++) {
      if (lp->txBufInfo[i].skb != NULL && lp->txBufInfo[i].eop) {
	 compat_dev_kfree_skb(lp->txBufInfo[i].skb, FREE_WRITE);
	 lp->txBufInfo[i].skb = NULL;
      }
   }

   for (i = 0; i < lp->numRxBuffers; i++) {
      if (lp->rxSkbuff[i] != NULL) {
	 compat_dev_kfree_skb(lp->rxSkbuff[i], FREE_WRITE);
	 lp->rxSkbuff[i] = NULL;
      }
   }

   COMPAT_NETDEV_MOD_DEC_USE_COUNT;

   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_load_multicast --
 *
 *      Load the multicast filter.
 *
 * Results:
 *      return number of entries used to compute LADRF
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int 
vmxnet_load_multicast (struct net_device *dev)
{
    Vmxnet_Private *lp = (Vmxnet_Private *) dev->priv;
    volatile u16 *mcast_table = (u16 *)lp->dd->LADRF;
    struct dev_mc_list *dmi = dev->mc_list;
    char *addrs;
    int i, j, bit, byte;
    u32 crc, poly = CRC_POLYNOMIAL_LE;

    /* clear the multicast filter */
    lp->dd->LADRF[0] = 0;
    lp->dd->LADRF[1] = 0;

    /* Add addresses */
    for (i = 0; i < dev->mc_count; i++){
	addrs = dmi->dmi_addr;
	dmi   = dmi->next;

	/* multicast address? */
	if (!(*addrs & 1))
	    continue;

	crc = 0xffffffff;
	for (byte = 0; byte < 6; byte++) {
	    for (bit = *addrs++, j = 0; j < 8; j++, bit >>= 1) {
		int test;

		test = ((bit ^ crc) & 0x01);
		crc >>= 1;

		if (test) {
		    crc = crc ^ poly;
		}
	    }
	 }

	 crc = crc >> 26;
	 mcast_table [crc >> 4] |= 1 << (crc & 0xf);
    }
    return i;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_set_multicast_list --
 *
 *      Network device set_multicast_list routine.  Called by Linux when the
 *      set of addresses to listen to changes, including both the multicast
 *      list and the broadcast, promiscuous, multicast, and allmulti flags.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Informs lower layer of the changes.
 *
 *-----------------------------------------------------------------------------
 */
static void 
vmxnet_set_multicast_list(struct net_device *dev)
{
   unsigned int ioaddr = dev->base_addr;
   Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;

   lp->dd->ifflags = ~(VMXNET_IFF_PROMISC
                      |VMXNET_IFF_BROADCAST
                      |VMXNET_IFF_MULTICAST);

   if (dev->flags & IFF_PROMISC) {
      printk(KERN_DEBUG "%s: Promiscuous mode enabled.\n", dev->name);
      lp->dd->ifflags |= VMXNET_IFF_PROMISC;
   }
   if (dev->flags & IFF_BROADCAST) {
      lp->dd->ifflags |= VMXNET_IFF_BROADCAST;
   }

   if (dev->flags & IFF_ALLMULTI) {
      lp->dd->LADRF[0] = 0xffffffff;
      lp->dd->LADRF[1] = 0xffffffff;
      lp->dd->ifflags |= VMXNET_IFF_MULTICAST;
   } else {
      if (vmxnet_load_multicast(dev)) {
         lp->dd->ifflags |= VMXNET_IFF_MULTICAST;
      }
   }
   outl(VMXNET_CMD_UPDATE_LADRF, ioaddr + VMXNET_COMMAND_ADDR);	       

   outl(VMXNET_CMD_UPDATE_IFF, ioaddr + VMXNET_COMMAND_ADDR);
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_set_mac_address --
 *
 *      Network device set_mac_address routine.  Called by Linux when someone
 *      asks to change the interface's MAC address.
 *
 * Results:
 *      0 for success; -EBUSY if interface is up.
 *
 * Side effects:
 *
 *-----------------------------------------------------------------------------
 */
static int
vmxnet_set_mac_address(struct net_device *dev, void *p)
{
   struct sockaddr *addr=p;
   unsigned int ioaddr = dev->base_addr;
   int i;

   if (netif_running(dev))
      return -EBUSY;

   memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

   for (i = 0; i < ETH_ALEN; i++) {
      outb(addr->sa_data[i], ioaddr + VMXNET_MAC_ADDR + i);
   }
   return 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * vmxnet_get_stats --
 *
 *      Network device get_stats routine.  Called by Linux when interface
 *      statistics are requested.
 *
 * Results:
 *      Returns a pointer to our private stats structure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static struct net_device_stats *
vmxnet_get_stats(struct net_device *dev)
{
   Vmxnet_Private *lp = (Vmxnet_Private *)dev->priv;

   return &lp->stats;
}

module_init(vmxnet_init);
module_exit(vmxnet_exit);
MODULE_DEVICE_TABLE(pci, vmxnet_chips);
MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION("VMware Virtual Ethernet driver.");
