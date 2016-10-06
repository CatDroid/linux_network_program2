#ifndef __COMPAT_IOPORT_H__
#   define __COMPAT_IOPORT_H__


#include <linux/ioport.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
static inline void *
compat_request_region(unsigned long start, unsigned long len, const char *name)
{
   if (check_region(start, len)) {
      return NULL;
   }
   request_region(start, len, name);
   return (void*)1;
}
#else
#define compat_request_region(start, len, name) request_region(start, len, name)
#endif


#endif /* __COMPAT_IOPORT_H__ */
