
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

#define NV_DMA_ACCES_RW 0
#define NV_DMA_ACCES_RO 1
#define NV_DMA_ACCES_WO 2
#define NV_DMA_TARGET_VIDMEM 0
/*
#define NV_DMA_TARGET_VIDMEM_TILED 1
#define NV_DMA_TARGET_PCI 2
*/
#define NV_DMA_TARGET_AGP 3
    
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

extern void NVDmaStart(NVPtr pNv, uint32_t object, uint32_t tag, int size);

#define NVDmaNext(pNv, data) do {                        \
	NVDEBUG("\tNVDmaNext: @0x%08x  0x%08x\n", ((pNv)->dmaCurrent),(data));           \
	(pNv)->dmaBase[(pNv)->dmaCurrent++] = (data);       \
} while(0)

#define NVDmaFloat(pNv, data) do { \
	union {                    \
		float v;           \
		uint32_t u;        \
	} c;                       \
	c.v = (data);              \
	NVDmaNext((pNv), c.u);     \
} while(0)

#define SURFACE_FORMAT                                              0x00000300
#define SURFACE_FORMAT_Y8                                           0x00000001
#define SURFACE_FORMAT_X1R5G5B5                                     0x00000002
#define SURFACE_FORMAT_R5G6B5                                       0x00000004
#define SURFACE_FORMAT_X8R8G8B8                                     0x00000006
#define SURFACE_FORMAT_A8R8G8B8                                     0x0000000a
#define SURFACE_PITCH                                               0x00000304
#define SURFACE_PITCH_SRC                                           15:0
#define SURFACE_PITCH_DST                                           31:16
#define SURFACE_OFFSET_SRC                                          0x00000308
#define SURFACE_OFFSET_DST                                          0x0000030C

#define ROP_SET                                                     0x00000300

#define PATTERN_FORMAT                                              0x00000300
#define PATTERN_FORMAT_DEPTH8                                       0x00000003
#define PATTERN_FORMAT_DEPTH16                                      0x00000001
#define PATTERN_FORMAT_DEPTH24                                      0x00000003
#define PATTERN_COLOR_0                                             0x00000310
#define PATTERN_COLOR_1                                             0x00000314
#define PATTERN_PATTERN_0                                           0x00000318
#define PATTERN_PATTERN_1                                           0x0000031C

#define CLIP_POINT                                                  0x00000300
#define CLIP_POINT_X                                                15:0
#define CLIP_POINT_Y                                                31:16
#define CLIP_SIZE                                                   0x00000304
#define CLIP_SIZE_WIDTH                                             15:0
#define CLIP_SIZE_HEIGHT                                            31:16

#define LINE_FORMAT                                                 0x00000300
#define LINE_FORMAT_DEPTH8                                          0x00000003
#define LINE_FORMAT_DEPTH16                                         0x00000001
#define LINE_FORMAT_DEPTH24                                         0x00000003
#define LINE_COLOR                                                  0x00000304
#define LINE_MAX_LINES                                              16
#define LINE_LINES(i)                                               0x00000400\
                                                                    +(i)*8
#define LINE_LINES_POINT0_X                                         15:0
#define LINE_LINES_POINT0_Y                                         31:16 
#define LINE_LINES_POINT1_X                                         47:32
#define LINE_LINES_POINT1_Y                                         63:48

#define BLIT_POINT_SRC                                              0x00000300
#define BLIT_POINT_SRC_X                                            15:0
#define BLIT_POINT_SRC_Y                                            31:16
#define BLIT_POINT_DST                                              0x00000304
#define BLIT_POINT_DST_X                                            15:0
#define BLIT_POINT_DST_Y                                            31:16
#define BLIT_SIZE                                                   0x00000308
#define BLIT_SIZE_WIDTH                                             15:0
#define BLIT_SIZE_HEIGHT                                            31:16

#define RECT_FORMAT                                                 0x00000300
#define RECT_FORMAT_DEPTH8                                          0x00000003
#define RECT_FORMAT_DEPTH16                                         0x00000001
#define RECT_FORMAT_DEPTH24                                         0x00000003
#define RECT_SOLID_COLOR                                            0x000003FC
#define RECT_SOLID_RECTS_MAX_RECTS                                  32
#define RECT_SOLID_RECTS(i)                                         0x00000400\
                                                                    +(i)*8
#define RECT_SOLID_RECTS_Y                                          15:0
#define RECT_SOLID_RECTS_X                                          31:16
#define RECT_SOLID_RECTS_HEIGHT                                     47:32
#define RECT_SOLID_RECTS_WIDTH                                      63:48

#define RECT_EXPAND_ONE_COLOR_CLIP                                  0x000007EC
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT0_X                         15:0
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT0_Y                         31:16
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT1_X                         47:32
#define RECT_EXPAND_ONE_COLOR_CLIP_POINT1_Y                         63:48
#define RECT_EXPAND_ONE_COLOR_COLOR                                 0x000007F4
#define RECT_EXPAND_ONE_COLOR_SIZE                                  0x000007F8
#define RECT_EXPAND_ONE_COLOR_SIZE_WIDTH                            15:0
#define RECT_EXPAND_ONE_COLOR_SIZE_HEIGHT                           31:16
#define RECT_EXPAND_ONE_COLOR_POINT                                 0x000007FC
#define RECT_EXPAND_ONE_COLOR_POINT_X                               15:0
#define RECT_EXPAND_ONE_COLOR_POINT_Y                               31:16
#define RECT_EXPAND_ONE_COLOR_DATA_MAX_DWORDS                       128
#define RECT_EXPAND_ONE_COLOR_DATA(i)                               0x00000800\
                                                                    +(i)*4

#define RECT_EXPAND_TWO_COLOR_CLIP                                  0x00000BE4
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT0_X                         15:0
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT0_Y                         31:16
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT1_X                         47:32
#define RECT_EXPAND_TWO_COLOR_CLIP_POINT1_Y                         63:48
#define RECT_EXPAND_TWO_COLOR_COLOR_0                               0x00000BEC
#define RECT_EXPAND_TWO_COLOR_COLOR_1                               0x00000BF0
#define RECT_EXPAND_TWO_COLOR_SIZE_IN                               0x00000BF4
#define RECT_EXPAND_TWO_COLOR_SIZE_IN_WIDTH                         15:0
#define RECT_EXPAND_TWO_COLOR_SIZE_IN_HEIGHT                        31:16
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT                              0x00000BF8
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT_WIDTH                        15:0
#define RECT_EXPAND_TWO_COLOR_SIZE_OUT_HEIGHT                       31:16
#define RECT_EXPAND_TWO_COLOR_POINT                                 0x00000BFC
#define RECT_EXPAND_TWO_COLOR_POINT_X                               15:0
#define RECT_EXPAND_TWO_COLOR_POINT_Y                               31:16
#define RECT_EXPAND_TWO_COLOR_DATA_MAX_DWORDS                       128
#define RECT_EXPAND_TWO_COLOR_DATA(i)                               0x00000C00\
                                                                    +(i)*4

#define STRETCH_BLIT_FORMAT                                         0x00000300
#define STRETCH_BLIT_FORMAT_DEPTH8                                  0x00000004
#define STRETCH_BLIT_FORMAT_DEPTH16                                 0x00000007
#define STRETCH_BLIT_FORMAT_DEPTH24                                 0x00000004
#define STRETCH_BLIT_FORMAT_A8R8G8B8                                0x00000003
#define STRETCH_BLIT_FORMAT_X8R8G8B8                                0x00000004
#define STRETCH_BLIT_FORMAT_YUYV                                    0x00000005
#define STRETCH_BLIT_FORMAT_UYVY                                    0x00000006
#define STRETCH_BLIT_OPERATION                                      0x00000304
#define STRETCH_BLIT_OPERATION_ROP                                  0x00000001
#define STRETCH_BLIT_OPERATION_COPY                                 0x00000003
#define STRETCH_BLIT_OPERATION_BLEND                                0x00000002
#define STRETCH_BLIT_CLIP_POINT                                     0x00000308
#define STRETCH_BLIT_CLIP_POINT_X                                   15:0 
#define STRETCH_BLIT_CLIP_POINT_Y                                   31:16
#define STRETCH_BLIT_CLIP_POINT                                     0x00000308
#define STRETCH_BLIT_CLIP_SIZE                                      0x0000030C
#define STRETCH_BLIT_CLIP_SIZE_WIDTH                                15:0
#define STRETCH_BLIT_CLIP_SIZE_HEIGHT                               31:16
#define STRETCH_BLIT_DST_POINT                                      0x00000310
#define STRETCH_BLIT_DST_POINT_X                                    15:0
#define STRETCH_BLIT_DST_POINT_Y                                    31:16
#define STRETCH_BLIT_DST_SIZE                                       0x00000314
#define STRETCH_BLIT_DST_SIZE_WIDTH                                 15:0
#define STRETCH_BLIT_DST_SIZE_HEIGHT                                31:16
#define STRETCH_BLIT_DU_DX                                          0x00000318
#define STRETCH_BLIT_DV_DY                                          0x0000031C
#define STRETCH_BLIT_SRC_SIZE                                       0x00000400
#define STRETCH_BLIT_SRC_SIZE_WIDTH                                 15:0
#define STRETCH_BLIT_SRC_SIZE_HEIGHT                                31:16
#define STRETCH_BLIT_SRC_FORMAT                                     0x00000404
#define STRETCH_BLIT_SRC_FORMAT_PITCH                               15:0
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN                              23:16
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN_CENTER                       0x00000001
#define STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER                       0x00000002
#define STRETCH_BLIT_SRC_FORMAT_FILTER                              31:24
#define STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE                 0x00000000
#define STRETCH_BLIT_SRC_FORMAT_FILTER_BILINEAR                     0x00000001
#define STRETCH_BLIT_SRC_OFFSET                                     0x00000408
#define STRETCH_BLIT_SRC_POINT                                      0x0000040C
#define STRETCH_BLIT_SRC_POINT_U                                    15:0
#define STRETCH_BLIT_SRC_POINT_V                                    31:16


#define MEMFORMAT_NOTIFY                                0x00000104
#define MEMFORMAT_DMA_NOTIFY                            0x00000180
#define MEMFORMAT_DMA_OBJECT_IN                         0x00000184
#define MEMFORMAT_DMA_OBJECT_OUT                        0x00000188
#define MEMFORMAT_OFFSET_IN                             0x0000030C
#define MEMFORMAT_OFFSET_OUT                            0x00000310
#define MEMFORMAT_PITCH_IN                              0x00000314
#define MEMFORMAT_PITCH_OUT                             0x00000318
#define MEMFORMAT_LINE_LENGTH_IN                        0x0000031C
#define MEMFORMAT_LINE_COUNT                            0x00000320


#endif /* NV_DMA_H */
