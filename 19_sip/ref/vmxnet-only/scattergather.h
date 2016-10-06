/* **********************************************************
 * Copyright 2000 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/*
 * scattergather.h --
 *
 *	Scatter gather structure.
 */


#ifndef _SCATTER_GATHER_H
#define _SCATTER_GATHER_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_defs.h"

#define SG_DEFAULT_LENGTH	16

typedef struct SG_Elem {
   uint64	offset;
   uint64 	addr;
   uint32	length;
   uint32       _pad;   
} SG_Elem;

typedef enum SG_AddrType {
   SG_MACH_ADDR,
   SG_PHYS_ADDR,
   SG_VIRT_ADDR,
} SG_AddrType;

#define SG_ARRAY_HEADER                         \
   SG_AddrType	addrType;                       \
   uint32	length;                         \
   uint32	maxLength;                      \
   uint32       _pad;                           \

typedef struct SG_Array {
   SG_ARRAY_HEADER
   SG_Elem	sg[SG_DEFAULT_LENGTH];
} SG_Array;

/*
 *-----------------------------------------------------------------------------
 *
 * SG_InitArray --
 *
 *      Init an SG_Array allocated on the stack
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Fill in sgArr->{length,maxLength} and make addrType invalid by default
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
SG_InitArray(SG_Array *sg, uint32 sgLen)
{
   sg->addrType = (SG_AddrType)-1;
   sg->length = 0;
   sg->maxLength = MAX(sgLen, SG_DEFAULT_LENGTH);
}

/*
 *-----------------------------------------------------------------------------
 *
 * SG_TotalLength --
 *
 *      Sum the lengths of the elements in the array.
 *
 * Results:
 *      Returns total length of all elements in the array.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
SG_TotalLength(SG_Array *sgArr)
{
   uint32 totLen = 0;
   uint32 i;

   for (i = 0; i < sgArr->length; i++) {
      totLen += sgArr->sg[i].length;
   }

   return totLen;
}

/*
 *-----------------------------------------------------------------------------
 *
 * SG_CollapseContig --
 *
 *      Collapse any contiguous array entries.
 *
 *      XXX This really only exists for scsi coredumping code (pr 56454)
 *          and might be better confined to that code.
 *
 * Results:
 *      Will decrease the number of array elements used if the range
 *      described spans contiguous regions.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
SG_CollapseContig(SG_Array *sgArr)
{
   uint32 i,j;

   for (i = 1, j = 0; i < sgArr->length; i++) {
      if (sgArr->sg[i].addr == (sgArr->sg[j].addr + sgArr->sg[j].length)) {
         sgArr->sg[j].length += sgArr->sg[i].length;
      } else {
         j++;
      }
   }
   sgArr->length = j + 1;
}

#endif
