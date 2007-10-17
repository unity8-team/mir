
 /***************************************************************************\
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_dma.h,v 1.4 2004/03/20 01:52:16 mvojkovi Exp $ */
#ifndef NV_DMA_H
#define NV_DMA_H

#define NVDEBUG if (NV_DMA_DEBUG) ErrorF

enum DMAObjects {
	NvNullObject		= 0x00000000,
	NvContextSurfaces	= 0x80000010, 
	NvRop			= 0x80000011, 
	NvImagePattern		= 0x80000012, 
	NvClipRectangle		= 0x80000013, 
	NvSolidLine		= 0x80000014, 
	NvImageBlit		= 0x80000015, 
	NvRectangle		= 0x80000016, 
	NvScaledImage		= 0x80000017, 
	NvMemFormat		= 0x80000018,
	Nv3D			= 0x80000019,
	NvImageFromCpu		= 0x8000001A,
	NvContextBeta1		= 0x8000001B,
	NvContextBeta4		= 0x8000001C,
	Nv2D			= 0x80000020,
	NvDmaFB			= 0xD8000001,
	NvDmaTT			= 0xD8000002,
	NvDmaNotifier0		= 0xD8000003,

	/*XVideo notifiers need to have consecutive handles, be careful when remapping*/
	NvDmaXvNotifier0	= 0xE8000000,
	NvDmaXvNotifier1	= 0xE8000001,
	NvDmaXvNotifier2	= 0xE8000002,
	NvDmaXvNotifier3	= 0xE8000003,
	NvDmaXvNotifier4	= 0xE8000004,
	NvDmaXvNotifier5	= 0xE8000005,
};

extern void NVDmaStartNNN(NVPtr pNv, uint32_t object, uint32_t tag, int size);

#define BEGIN_RING(obj,mthd,size) do {                                         \
	NVDmaStartNNN(pNv, (obj), (mthd), (size));                             \
} while(0)

#define OUT_RING(data) do {                                                    \
	NVDEBUG("\tOUT_RING  : @0x%08x  0x%08x\n",                             \
		(unsigned)(pNv->dmaCurrent), (unsigned)(data));                \
	pNv->dmaBase[pNv->dmaCurrent++] = (data);                              \
} while(0)

#define OUT_RINGf(data) do {                                                   \
	union { float v; uint32_t u; } c;                                      \
	c.v = (data);                                                          \
	OUT_RING(c.u);                                                         \
} while(0)

#define WAIT_RING(size) do {                                                   \
	NVDmaWaitNNN(pScrn, (size));                                           \
} while(0)

#define FIRE_RING() do {                                                       \
	NVDmaKickoffNNN(pNv);                                                  \
} while(0)

#endif /* NV_DMA_H */
