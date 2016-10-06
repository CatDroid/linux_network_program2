/* **********************************************************
 * Copyright 1998 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/*
 * Sets the proper defines from the Linux header files
 *
 * This file must be included before the inclusion of any kernel header file,
 * with the exception of linux/autoconf.h and linux/version.h --hpreg
 */

#ifndef __VMX_CONFIG_H__
#define __VMX_CONFIG_H__

#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMNIXMOD
#include "includeCheck.h"

#include <linux/autoconf.h>
#include "compat_version.h"

/* We rely on Kernel Module support.  Check here. */
#ifndef CONFIG_MODULES
#error "No Module support in this kernel.  Please configure with CONFIG_MODULES"
#endif

/*
 * 2.2 kernels still use __SMP__ (derived from CONFIG_SMP
 * in the main Makefile), so we do it here.
 */

#ifdef CONFIG_SMP
   #define __SMP__ 1
#endif

#if defined(CONFIG_MODVERSIONS) && defined(KERNEL_2_1)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,60)
/* MODVERSIONS might be already defined when using kernel's Makefiles */
#ifndef MODVERSIONS
#define MODVERSIONS
#endif
#include <linux/modversions.h>
#endif
#endif

#ifndef __KERNEL__
#define __KERNEL__
#endif

#endif
