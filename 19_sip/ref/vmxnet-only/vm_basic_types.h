/* **********************************************************
 * Copyright (c) 1998-2007 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 *
 * vm_basic_types.h --
 *
 *    basic data types.
 */


#ifndef _VM_BASIC_TYPES_H_
#define _VM_BASIC_TYPES_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

/* STRICT ANSI means the Xserver build and X defines Bool differently. */
#if !defined(__STRICT_ANSI__) || defined(__FreeBSD__)
typedef char           Bool;
#endif

#ifndef FALSE
#define FALSE          0
#endif

#ifndef TRUE
#define TRUE           1
#endif

#define IsBool(x)      (((x) & ~1) == 0)
#define IsBool2(x, y)  ((((x) | (y)) & ~1) == 0)

/*
 * Macros __i386__ and __ia64 are intrinsically defined by GCC
 */
#ifdef __i386__
#define VM_I386
#endif

#ifdef __ia64__
#define VM_IA64
#endif

#ifdef _WIN64
#define __x86_64__
#endif

#ifdef __x86_64__
#define VM_X86_64
#define VM_I386
#define vm_x86_64 (1)
#else
#define vm_x86_64 (0)
#endif



#ifdef _WIN32
/* safe assumption for a while */
#define VM_I386
#endif

#if defined VM_I386 && defined VM_IA64
#error "Only one CPU platform is allowed."
#endif

#ifdef _MSC_VER
typedef unsigned __int64 uint64;
typedef signed __int64 int64;

#pragma warning (3 :4505) // unreferenced local function
#pragma warning (disable :4018) // signed/unsigned mismatch
#pragma warning (disable :4761) // integral size mismatch in argument; conversion supplied
#pragma warning (disable :4305) // truncation from 'const int' to 'short'
#pragma warning (disable :4244) // conversion from 'unsigned short' to 'unsigned char'
#if !defined VMX86_DEVEL // XXX until we clean up all the code -- edward
#pragma warning (disable :4133) // incompatible types - from 'struct VM *' to 'int *'
#pragma warning (disable :4047) // differs in levels of indirection
#endif
#pragma warning (disable :4146) // unary minus operator applied to unsigned type, result still unsigned
#pragma warning (disable :4142) // benign redefinition of type

#elif __GNUC__
/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
#if defined(VM_IA64) || defined(VM_X86_64)
typedef unsigned long uint64;
typedef long int64;
#else
typedef unsigned long long uint64;
typedef long long int64;
#endif
#elif __FreeBSD__
typedef unsigned long long uint64;
typedef long long int64;
#endif
#else
#error - Need compiler define for int64/uint64
#endif

typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;

typedef int       int32;
typedef short     int16;
typedef char      int8;

/*
 * FreeBSD (for the tools build) unconditionally defines these in
 * sys/inttypes.h so don't redefine them if this file has already
 * been included. [greg]
 *
 * This applies to Solaris as well.
 */

#if defined(__FreeBSD__) || defined(sun)
#   ifdef KLD_MODULE
#      include <sys/types.h>
#   else
#      if BSD_VERSION >= 50
#         include <inttypes.h>
#         include <sys/types.h>
#      else
#         include <sys/inttypes.h>
#      endif
#   endif
#elif defined __APPLE__
#   if KERNEL
#       include <sys/types.h> /* mostly for size_t */
#       include <stdint.h>
#   else
#       include <inttypes.h>
#       include <stdlib.h>
#       include <stdint.h>
#   endif
#else
#   if !defined(__intptr_t_defined) && !defined(intptr_t)
#      define __intptr_t_defined
#      define intptr_t  intptr_t
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef int64     intptr_t;
#         else
             typedef int32     intptr_t;
#         endif
#      endif

#      ifdef VM_IA64
          typedef int64     intptr_t;
#      endif
#   endif

#   ifndef _STDINT_H
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef uint64    uintptr_t;
#         else
             typedef uint32    uintptr_t;
#         endif
#      endif

#      ifdef VM_IA64
          typedef uint64    uintptr_t;
#      endif
#   endif
#endif


/*
 * Time
 * XXX These should be cleaned up.  -- edward
 */

typedef int64 VmTimeType;          /* Time in microseconds */
typedef int64 VmTimeRealClock;     /* Real clock kept in microseconds */
typedef int64 VmTimeVirtualClock;  /* Virtual Clock kept in CPU cycles */

/*
 * Printf format specifiers for size_t and 64-bit number.
 * Use them like this:
 *    printf("%"FMT64"d\n", big);
 */

#ifdef _MSC_VER
   #define FMTSZ      "I"
   #define FMT64      "I64"
#elif __GNUC__
   #if defined(N_PLAT_NLM) || defined(sun) || (defined(__FreeBSD__) \
                                               && __FreeBSD__ < 5)
      #ifdef VM_X86_64
         #define FMTSZ  "l"
      #else
         #define FMTSZ  ""
      #endif
   #else
      /* BSD/Darwin, Linux */
      #define FMTSZ     "z"
   #endif
   #ifdef VM_X86_64
      #define FMT64     "l"
   #elif defined(sun) || defined(__APPLE__) || defined(__FreeBSD__)
      #define FMT64     "ll"
   #else
      #define FMT64     "L"
   #endif
#else
   #error - Need compiler define for FMT64 and FMTSZ
#endif

/*
 * Suffix for 64-bit constants.  Use it like this:
 *    CONST64(0x7fffffffffffffff) for signed or
 *    CONST64U(0x7fffffffffffffff) for unsigned.
 *
 * 2004.08.30(thutt):
 *   The vmcore/asm64/gen* programs are compiled as 32-bit
 *   applications, but must handle 64 bit constants.  If the
 *   64-bit-constant defining macros are already defined, the
 *   definition will not be overwritten.
 */

#if !defined(CONST64) || !defined(CONST64U)
#ifdef _MSC_VER
#define CONST64(c) c##I64
#define CONST64U(c) c##uI64
#elif __GNUC__
#ifdef VM_X86_64
#define CONST64(c) c##L
#define CONST64U(c) c##uL
#else
#define CONST64(c) c##LL
#define CONST64U(c) c##uLL
#endif
#else
#error - Need compiler define for CONST64
#endif
#endif


#define MIN_INT32  ((int32)0x80000000)
#define MAX_INT32  ((int32)0x7fffffff)

#define MIN_UINT32 ((uint32)0)
#define MAX_UINT32 ((uint32)0xffffffff)

#define MIN_INT64  (CONST64(0x8000000000000000))
#define MAX_INT64  (CONST64(0x7fffffffffffffff))

#define MIN_UINT64 (CONST64U(0))
#define MAX_UINT64 (CONST64U(0xffffffffffffffff))


/*
 * Type big enough to hold an integer between 0..100
 */
typedef uint8 Percent;
#define asPercent(v)	((Percent)(v))
#define CHOOSE_PERCENT  asPercent(-1)


typedef uintptr_t VA;
typedef uintptr_t VPN;

typedef uint64    PA;
typedef uint32    PPN;

typedef uint64    PhysMemOff;
typedef uint64    PhysMemSize;

/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
typedef uint64    BA;
#endif
typedef uint32    BPN;
typedef uint32    PageNum;
typedef unsigned  MemHandle;
typedef int32     World_ID;

#define INVALID_WORLD_ID ((World_ID)0)

typedef World_ID User_CartelID;
#define INVALID_CARTEL_ID INVALID_WORLD_ID

/* world page number */
typedef uint32    WPN;

/* The Xserver source compiles with -ansi -pendantic */
#ifndef __STRICT_ANSI__
typedef uint64     MA;
typedef uint32     MPN;
#endif

/*
 * Linear address
 */

typedef uintptr_t LA;
typedef uintptr_t LPN;
#define LA_2_LPN(_la)     ((_la) >> PAGE_SHIFT)
#define LPN_2_LA(_lpn)    ((_lpn) << PAGE_SHIFT)

#define LAST_LPN   ((((LA)  1) << (8 * sizeof(LA)   - PAGE_SHIFT)) - 1)
#define LAST_LPN32 ((((LA32)1) << (8 * sizeof(LA32) - PAGE_SHIFT)) - 1)
#define LAST_LPN64 ((((LA64)1) << (8 * sizeof(LA64) - PAGE_SHIFT)) - 1)

/* Valid bits in a LPN. */
#define LPN_MASK   LAST_LPN
#define LPN_MASK32 LAST_LPN32
#define LPN_MASK64 LAST_LPN64

/*
 * On 64 bit platform, address and page number types default
 * to 64 bit. When we need to represent a 32 bit address, we use
 * types defined below.
 *
 * On 32 bit platform, the following types are the same as the
 * default types.
 */
typedef uint32 VA32;
typedef uint32 VPN32;
typedef uint32 LA32;
typedef uint32 LPN32;
typedef uint32 PA32;
typedef uint32 PPN32;
typedef uint32 MA32;
typedef uint32 MPN32;

/*
 * On 64 bit platform, the following types are the same as the
 * default types.
 */
typedef uint64 VA64;
typedef uint64 VPN64;
typedef uint64 LA64;
typedef uint64 LPN64;
typedef uint64 PA64;
typedef uint64 PPN64;
typedef uint64 MA64;
typedef uint64 MPN64;

/*
 * VA typedefs for user world apps.
 */
typedef VA32 UserVA32;
typedef UserVA32 UserVAConst; /* Userspace ptr to data that we may only read. */
#ifdef VMKERNEL
typedef UserVA32 UserVA;
#else
typedef void * UserVA;
#endif


#define MAX_PPN     ((PPN)0x0fffffff)   /* Maximal observable PPN value. */
#define INVALID_PPN ((PPN)-1)

#define INVALID_BPN  ((BPN) 0x3fffffff) /* BPNs don't use the high two bits. */

#define INVALID_MPN  ((MPN)-1)
#define RESERVED_MPN ((MPN) 0)
/* Support 39 bits of address space, minus one page. */
#define MAX_MPN      ((MPN) 0x07ffffff)

#define INVALID_LPN ((LPN)-1)
#define INVALID_VPN ((VPN)-1)
#define INVALID_LPN64 ((LPN64)-1)
#define INVALID_PAGENUM ((PageNum)-1)
#define INVALID_WPN ((WPN) -1)


/*
 * Format modifier for printing VA, LA, and VPN.
 * Use them like this: Log("%#"FMTLA"x\n", laddr)
 */

#if defined(VMM64) || defined(FROBOS64) || vm_x86_64 || defined __APPLE__
#   define FMTLA "l"
#   define FMTVA "l"
#   define FMTVPN "l"
#else
#   define FMTLA ""
#   define FMTVA ""
#   define FMTVPN ""
#endif


#define EXTERN        extern
#define CONST         const


#ifndef INLINE
#   ifdef _MSC_VER
#      define INLINE        __inline
#   else
#      define INLINE        inline
#   endif
#endif


/*
 * Annotation for data that may be exported into a DLL and used by other
 * apps that load that DLL and import the data.
 */
#if defined(_WIN32) && defined(VMX86_IMPORT_DLLDATA)
#  define VMX86_EXTERN_DATA       extern __declspec(dllimport)
#else // !_WIN32
#  define VMX86_EXTERN_DATA       extern
#endif

#if defined(_WIN32) && !defined(VMX86_NO_THREADS)
#define THREADSPECIFIC __declspec(thread)
#else
#define THREADSPECIFIC
#endif


/*
 * Consider the following reasons functions are inlined:
 *
 *  1) inlined for performance reasons
 *  2) inlined because it's a single-use function
 *
 * Functions which meet only condition 2 should be marked with this
 * inline macro; It is not critical to be inlined (but there is a
 * code-space & runtime savings by doing so), so when other callers
 * are added the inline-ness should be removed.
 */

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)
/*
 * Starting at version 3.3, gcc does not always inline functions marked
 * 'inline' (it depends on their size). To force gcc to do so, one must use the
 * extra __always_inline__ attribute.
 */
#   define INLINE_SINGLE_CALLER INLINE __attribute__((__always_inline__))
#   if    defined(VMM) \
       && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 4))
#      warning Verify INLINE_SINGLE_CALLER '__always_inline__' attribute (did \
             monitor size change?)
#   endif
#else
#   define INLINE_SINGLE_CALLER INLINE
#endif

/*
 * Used when a hard guaranteed of no inlining is needed. Very few
 * instances need this since the absence of INLINE is a good hint
 * that gcc will not do inlining.
 */

#if defined(__GNUC__) && defined(VMM)
#define ABSOLUTELY_NOINLINE __attribute__((__noinline__))
#endif

/*
 * Attributes placed on function declarations to tell the compiler
 * that the function never returns.
 */

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#elif __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 9)
#define NORETURN __attribute__((__noreturn__))
#else
#define NORETURN
#endif

/*
 * GCC 3.2 inline asm needs the + constraint for input/ouput memory operands.
 * Older GCCs don't know about it --hpreg
 */

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)
#   define VM_ASM_PLUS 1
#else
#   define VM_ASM_PLUS 0
#endif

/*
 * Branch prediction hints:
 *     LIKELY(exp)   - Expression exp is likely TRUE.
 *     UNLIKELY(exp) - Expression exp is likely FALSE.
 *   Usage example:
 *        if (LIKELY(excCode == EXC_NONE)) {
 *               or
 *        if (UNLIKELY(REAL_MODE(vc))) {
 *
 * We know how to predict branches on gcc3 and later (hopefully),
 * all others we don't so we do nothing.
 */

#if (__GNUC__ >= 3)
/*
 * gcc3 uses __builtin_expect() to inform the compiler of an expected value.
 * We use this to inform the static branch predictor. The '!!' in LIKELY
 * will convert any !=0 to a 1.
 */
#define LIKELY(_exp)     __builtin_expect(!!(_exp), 1)
#define UNLIKELY(_exp)   __builtin_expect((_exp), 0)
#else
#define LIKELY(_exp)      (_exp)
#define UNLIKELY(_exp)    (_exp)
#endif

/*
 * GCC's argument checking for printf-like functions
 * This is conditional until we have replaced all `"%x", void *'
 * with `"0x%08x", (uint32) void *'. Note that %p prints different things
 * on different platforms.  Argument checking is enabled for the
 * vmkernel, which has already been cleansed.
 *
 * fmtPos is the position of the format string argument, beginning at 1
 * varPos is the position of the variable argument, beginning at 1
 */

#if defined(__GNUC__)
# define PRINTF_DECL(fmtPos, varPos) __attribute__((__format__(__printf__, fmtPos, varPos)))
#else
# define PRINTF_DECL(fmtPos, varPos)
#endif

/*
 * UNUSED_PARAM should surround the parameter name and type declaration,
 * e.g. "int MyFunction(int var1, UNUSED_PARAM(int var2))"
 *
 */

#ifndef UNUSED_PARAM
# if defined(__GNUC__)
#  define UNUSED_PARAM(_parm) _parm  __attribute__((__unused__))
# else
#  define UNUSED_PARAM(_parm) _parm
# endif
#endif

/*
 * REGPARM defaults to REGPARM3, i.e., a requent that gcc
 * puts the first three arguments in registers.  (It is fine
 * if the function has fewer than three args.)  Gcc only.
 * Syntactically, put REGPARM where you'd put INLINE or NORETURN.
 */

#if defined(__GNUC__)
# define REGPARM0 __attribute__((regparm(0)))
# define REGPARM1 __attribute__((regparm(1)))
# define REGPARM2 __attribute__((regparm(2)))
# define REGPARM3 __attribute__((regparm(3)))
# define REGPARM REGPARM3
#else
# define REGPARM0
# define REGPARM1
# define REGPARM2
# define REGPARM3
# define REGPARM
#endif

/*
 * ALIGNED specifies minimum alignment in "n" bytes.
 */

#ifdef __GNUC__
#define ALIGNED(n) __attribute__((__aligned__(n)))
#else
#define ALIGNED(n)
#endif


/*
 * Once upon a time, this was used to silence compiler warnings that
 * get generated when the compiler thinks that a function returns
 * when it is marked noreturn.  Don't do it.  Use NOT_REACHED().
 */

#define INFINITE_LOOP()           do { } while (1)

/*
 * On FreeBSD (for the tools build), size_t is typedef'd if _BSD_SIZE_T_
 * is defined. Use the same logic here so we don't define it twice. [greg]
 */
#ifdef __FreeBSD__
#   ifdef _BSD_SIZE_T_
#      undef _BSD_SIZE_T_
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef uint64 size_t;
#         else
             typedef uint32 size_t;
#         endif
#      endif /* VM_I386 */

#      ifdef VM_IA64
             typedef uint64 size_t;
#      endif
#   endif
#else
#   ifndef _SIZE_T
#      define _SIZE_T
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef uint64 size_t;
#         else
             typedef uint32 size_t;
#         endif
#      endif /* VM_I386 */

#      ifdef VM_IA64
             typedef uint64 size_t;
#      endif
#   endif
#   if !defined(FROBOS) && !defined(_SSIZE_T) && !defined(ssize_t)  && !defined(__ssize_t_defined) && !defined(_SSIZE_T_DECLARED)
#      define _SSIZE_T
#      define __ssize_t_defined
#      define _SSIZE_T_DECLARED
#      ifdef VM_I386
#         ifdef VM_X86_64
             typedef int64 ssize_t;
#         else
             typedef int32 ssize_t;
#         endif
#      endif /* VM_I386 */

#      ifdef VM_IA64
             typedef int64 ssize_t;
#      endif
#   endif
#endif

/*
 * Format modifier for printing pid_t.  On sun the pid_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The pid is %"FMTPID".\n", pid);
 */
#ifdef sun
#   ifdef VM_X86_64
#      define FMTPID "d"
#   else
#      define FMTPID "lu"
#   endif
#else
# define FMTPID "d"
#endif

/*
 * Format modifier for printing uid_t.  On sun the uid_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The uid is %"FMTUID".\n", uid);
 */
#ifdef sun
#   ifdef VM_X86_64
#      define FMTUID "u"
#   else
#      define FMTUID "lu"
#   endif
#else
# define FMTUID "u"
#endif

/*
 * Format modifier for printing mode_t.  On sun the mode_t is a ulong, but on
 * Linux it's an int.
 * Use this like this: printf("The mode is %"FMTMODE".\n", mode);
 */
#ifdef sun
#   ifdef VM_X86_64
#      define FMTMODE "o"
#   else
#      define FMTMODE "lo"
#   endif
#else
# define FMTMODE "o"
#endif

#endif  /* _VM_BASIC_TYPES_H_ */
