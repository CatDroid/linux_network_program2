#ifndef __COMPAT_INTERRUPT_H__
#   define __COMPAT_INTERRUPT_H__


#include <linux/interrupt.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 69)
/*
 * We cannot just define irqreturn_t, as some 2.4.x kernels have
 * typedef void irqreturn_t; for "increasing" backward compatibility.
 */
typedef void compat_irqreturn_t;
#define COMPAT_IRQ_NONE
#define COMPAT_IRQ_HANDLED
#define COMPAT_IRQ_RETVAL(x)
#else
typedef irqreturn_t compat_irqreturn_t;
#define COMPAT_IRQ_NONE		IRQ_NONE
#define COMPAT_IRQ_HANDLED	IRQ_HANDLED
#define COMPAT_IRQ_RETVAL(x)	IRQ_RETVAL(x)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#define COMPAT_IRQF_DISABLED    SA_INTERRUPT
#define COMPAT_IRQF_SHARED      SA_SHIRQ
#else
#define COMPAT_IRQF_DISABLED    IRQF_DISABLED
#define COMPAT_IRQF_SHARED      IRQF_SHARED
#endif


#endif /* __COMPAT_INTERRUPT_H__ */
