/***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
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
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_hw.h,v 1.26 2003/07/31 20:24:31 mvojkovi Exp $ */
#ifndef __RIVA_HW_H__
#define __RIVA_HW_H__

/*
 * Define supported architectures.
 */
/***************************************************************************\
*                                                                           *
*                             FIFO registers.                               *
*                                                                           *
\***************************************************************************/

/*
 * Raster OPeration. Windows style ROP3.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BB];
    CARD32 Rop3;
} RivaRop;
/*
 * 8X8 Monochrome pattern.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BD];
    CARD32 Shape;
    CARD32 reserved03[0x001];
    CARD32 Color0;
    CARD32 Color1;
    CARD32 Monochrome[2];
} RivaPattern;
/*
 * Scissor clip rectangle.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BB];
    CARD32 TopLeft;
    CARD32 WidthHeight;
} RivaClip;
/*
 * 2D filled rectangle.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop[1];
    CARD32 reserved01[0x0BC];
    CARD32 Color;
    CARD32 reserved03[0x03E];
    CARD32 TopLeft;
    CARD32 WidthHeight;
} RivaRectangle;
/*
 * 2D screen-screen BLT.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BB];
    CARD32 TopLeftSrc;
    CARD32 TopLeftDst;
    CARD32 WidthHeight;
} RivaScreenBlt;
/*
 * 2D pixel BLT.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop[1];
    CARD32 reserved01[0x0BC];
    CARD32 TopLeft;
    CARD32 WidthHeight;
    CARD32 WidthHeightIn;
    CARD32 reserved02[0x03C];
    CARD32 Pixels;
} RivaPixmap;
/*
 * Filled rectangle combined with monochrome expand.  Useful for glyphs.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BB];
    CARD32 reserved03[(0x040)-1];
    CARD32 Color1A;
    struct
    {
        CARD32 TopLeft;
        CARD32 WidthHeight;
    } UnclippedRectangle[64];
    CARD32 reserved04[(0x080)-3];
    struct
    {
        CARD32 TopLeft;
        CARD32 BottomRight;
    } ClipB;
    CARD32 Color1B;
    struct
    {
        CARD32 TopLeft;
        CARD32 BottomRight;
    } ClippedRectangle[64];
    CARD32 reserved05[(0x080)-5];
    struct
    {
        CARD32 TopLeft;
        CARD32 BottomRight;
    } ClipC;
    CARD32 Color1C;
    CARD32 WidthHeightC;
    CARD32 PointC;
    CARD32 MonochromeData1C;
    CARD32 reserved06[(0x080)+121];
    struct
    {
        CARD32 TopLeft;
        CARD32 BottomRight;
    } ClipD;
    CARD32 Color1D;
    CARD32 WidthHeightInD;
    CARD32 WidthHeightOutD;
    CARD32 PointD;
    CARD32 MonochromeData1D;
    CARD32 reserved07[(0x080)+120];
    struct
    {
        CARD32 TopLeft;
        CARD32 BottomRight;
    } ClipE;
    CARD32 Color0E;
    CARD32 Color1E;
    CARD32 WidthHeightInE;
    CARD32 WidthHeightOutE;
    CARD32 PointE;
    CARD32 MonochromeData01E;
} RivaBitmap;
/*
 * 2D line.
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop[1];
    CARD32 reserved01[0x0BC];
    CARD32 Color;             /* source color               0304-0307*/
    CARD32 Reserved02[0x03e];
    struct {                /* start aliased methods in array   0400-    */
        CARD32 point0;        /* y_x S16_S16 in pixels            0-   3*/
        CARD32 point1;        /* y_x S16_S16 in pixels            4-   7*/
    } Lin[16];              /* end of aliased methods in array      -047f*/
    struct {                /* start aliased methods in array   0480-    */
        CARD32 point0X;       /* in pixels, 0 at left                0-   3*/
        CARD32 point0Y;       /* in pixels, 0 at top                 4-   7*/
        CARD32 point1X;       /* in pixels, 0 at left                8-   b*/
        CARD32 point1Y;       /* in pixels, 0 at top                 c-   f*/
    } Lin32[8];             /* end of aliased methods in array      -04ff*/
    CARD32 PolyLin[32];       /* y_x S16_S16 in pixels         0500-057f*/
    struct {                /* start aliased methods in array   0580-    */
        CARD32 x;             /* in pixels, 0 at left                0-   3*/
        CARD32 y;             /* in pixels, 0 at top                 4-   7*/
    } PolyLin32[16];        /* end of aliased methods in array      -05ff*/
    struct {                /* start aliased methods in array   0600-    */
        CARD32 color;         /* source color                     0-   3*/
        CARD32 point;         /* y_x S16_S16 in pixels            4-   7*/
    } ColorPolyLin[16];     /* end of aliased methods in array      -067f*/
} RivaLine;
/*
 * 2D/3D surfaces
 */
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BE];
    CARD32 Offset;
} RivaSurface;
typedef volatile struct
{
    CARD32 reserved00[4];
    CARD16 FifoFree;
    CARD16 Nop;
    CARD32 reserved01[0x0BD];
    CARD32 Pitch;
    CARD32 RenderBufferOffset;
    CARD32 ZBufferOffset;
} RivaSurface3D;
    
/***************************************************************************\
*                                                                           *
*                        Virtualized RIVA H/W interface.                    *
*                                                                           *
\***************************************************************************/

#define FP_ENABLE  1
#define FP_DITHER  2

struct _riva_hw_inst;
struct _riva_hw_state;
/*
 * Virtialized chip interface. Makes RIVA 128 and TNT look alike.
 */
typedef struct _riva_hw_inst
{
    /*
     * Chip specific settings.
     */
    CARD32 CrystalFreqKHz;
    CARD32 RamAmountKBytes;
    CARD32 MaxVClockFreqKHz;
    CARD32 RamBandwidthKBytesPerSec;
    CARD32 EnableIRQ;
    CARD32 IO;
    CARD32 VBlankBit;
    CARD32 FifoFreeCount;
    CARD32 FifoEmptyCount;
    CARD32 CursorStart;
    /*
     * Non-FIFO registers.
     */
    volatile CARD32 *PCRTC;
    volatile CARD32 *PFB;
    volatile CARD32 *PFIFO;
    volatile CARD32 *PGRAPH;
    volatile CARD32 *PEXTDEV;
    volatile CARD32 *PTIMER;
    volatile CARD32 *PMC;
    volatile CARD32 *PRAMIN;
    volatile CARD32 *FIFO;
    volatile CARD32 *CURSOR;
    volatile CARD8 *PCIO;
    volatile CARD8 *PVIO;
    volatile CARD8 *PDIO;
    volatile CARD32 *PRAMDAC;
    /*
     * Common chip functions.
     */
    int  (*Busy)(struct _riva_hw_inst *);
    void (*CalcStateExt)(struct _riva_hw_inst *,struct _riva_hw_state *,int,int,int,int,int,int);
    void (*LoadStateExt)(struct _riva_hw_inst *,struct _riva_hw_state *);
    void (*UnloadStateExt)(struct _riva_hw_inst *,struct _riva_hw_state *);
    void (*SetStartAddress)(struct _riva_hw_inst *,CARD32);
    int  (*ShowHideCursor)(struct _riva_hw_inst *,int);
    void (*LockUnlock)(struct _riva_hw_inst *, int);
    /*
     * Current extended mode settings.
     */
    struct _riva_hw_state *CurrentState;
    /*
     * FIFO registers.
     */
    RivaRop                 *Rop;
    RivaPattern             *Patt;
    RivaClip                *Clip;
    RivaPixmap              *Pixmap;
    RivaScreenBlt           *Blt;
    RivaBitmap              *Bitmap;
    RivaLine                *Line;
} RIVA_HW_INST;
/*
 * Extended mode state information.
 */
typedef struct _riva_hw_state
{
    CARD32 bpp;
    CARD32 width;
    CARD32 height;
    CARD32 interlace;
    CARD32 repaint0;
    CARD32 repaint1;
    CARD32 screen;
    CARD32 extra;
    CARD32 pixel;
    CARD32 horiz;
    CARD32 arbitration0;
    CARD32 arbitration1;
    CARD32 vpll;
    CARD32 pllsel;
    CARD32 general;
    CARD32 config;
    CARD32 cursorConfig;
    CARD32 cursor0;
    CARD32 cursor1;
    CARD32 cursor2;
    CARD32 offset;
    CARD32 pitch;
} RIVA_HW_STATE;

/*
 * FIFO Free Count. Should attempt to yield processor if RIVA is busy.
 */

#define RIVA_FIFO_FREE(hwinst,hwptr,cnt)                           \
{                                                                  \
   while ((hwinst).FifoFreeCount < (cnt)) {                          \
        mem_barrier(); \
        mem_barrier(); \
	(hwinst).FifoFreeCount = (hwinst).hwptr->FifoFree >> 2;        \
   } \
   (hwinst).FifoFreeCount -= (cnt);                                \
}
#define RIVA_BUSY(hwinst) \
{ \
   mem_barrier(); \
   while ((hwinst).Busy(&(hwinst))); \
}
#endif /* __RIVA_HW_H__ */

