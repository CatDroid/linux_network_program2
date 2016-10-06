/* **********************************************************
 * Copyright 1999 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/*
 * compat_init.h: Initialization compatibility wrappers.
 */

#ifndef __COMPAT_INIT_H__
#define __COMPAT_INIT_H__

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 0)
#include <linux/init.h>
#endif

#ifndef module_init
#define module_init(x) int init_module(void)     { return x(); }
#endif

#ifndef module_exit
#define module_exit(x) void cleanup_module(void) { x(); }
#endif

#endif /* __COMPAT_INIT_H__ */
