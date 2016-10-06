/*
 * Detect whether skb_linearize takes one or two arguments.
 */

#include <linux/autoconf.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 17)
/*
 * Since 2.6.18 all kernels have single-argument skb_linearize.  For
 * older kernels use autodetection.  Not using autodetection on newer
 * kernels saves us from compile failure on some post 2.6.18 kernels
 * which do not have selfcontained skbuff.h.
 */

#include <linux/skbuff.h>

int test_skb_linearize(struct sk_buff *skb)
{
   return skb_linearize(skb);
}

#endif
