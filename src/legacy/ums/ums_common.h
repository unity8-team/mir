
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 David Dawes

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 */

#ifndef _INTEL_COMMON_H_
#define _INTEL_COMMON_H_

/* Provide substitutes for gcc's __FUNCTION__ on other compilers */
#if !defined(__GNUC__) && !defined(__FUNCTION__)
# if defined(__STDC__) && (__STDC_VERSION__>=199901L) /* C99 */
#  define __FUNCTION__ __func__
# else
#  define __FUNCTION__ ""
# endif
#endif


#define PFX __FILE__,__LINE__,__FUNCTION__
#define FUNCTION_NAME __FUNCTION__

#ifdef I830DEBUG
#define MARKER() ErrorF("\n### %s:%d: >>> %s <<< ###\n\n", \
			 __FILE__, __LINE__,__FUNCTION__)
#define DPRINTF I830DPRINTF
#else /* #ifdef I830DEBUG */
#define MARKER()
#define DPRINTF I830DPRINTF_stub
static inline void
I830DPRINTF_stub(const char *filename, int line, const char *function,
		 const char *fmt, ...)
{
}
#endif /* #ifdef I830DEBUG */

#define KB(x) ((x) * 1024)
#define MB(x) ((x) * KB(1024))

/* Using usleep() makes things noticably slow. */
#if 0
#define DELAY(x) usleep(x)
#else
#define DELAY(x) do {;} while (0)
#endif

/* I830 hooks for the I810 driver setup/probe. */

extern void I830DPRINTF_stub(const char *filename, int line,
			     const char *function, const char *fmt, ...);

#define PrintErrorState ums_dump_error_state
#define WaitRingFunc ums_I830WaitLpRing
#define RecPtr pI830

static inline void memset_volatile(volatile void *b, int c, size_t len)
{
    int i;
    
    for (i = 0; i < len; i++)
	((volatile char *)b)[i] = c;
}

static inline void memcpy_volatile(volatile void *dst, const void *src,
				   size_t len)
{
    int i;
    
    for (i = 0; i < len; i++)
	((volatile char *)dst)[i] = ((volatile char *)src)[i];
}

/* Memory mapped register access macros */
#define INREG8(addr)        *(volatile uint8_t *)(RecPtr->MMIOBase + (addr))
#define INREG16(addr)       *(volatile uint16_t *)(RecPtr->MMIOBase + (addr))
#define INREG(addr)         *(volatile uint32_t *)(RecPtr->MMIOBase + (addr))
#define INGTT(addr)         *(volatile uint32_t *)(RecPtr->GTTBase + (addr))
#define POSTING_READ(addr)  (void)INREG(addr)

#define OUTREG8(addr, val) do {						\
   *(volatile uint8_t *)(RecPtr->MMIOBase  + (addr)) = (val);		\
   if (UMS_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG8(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

#define OUTREG16(addr, val) do {					\
   *(volatile uint16_t *)(RecPtr->MMIOBase + (addr)) = (val);		\
   if (UMS_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG16(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

#define OUTREG(addr, val) do {						\
   *(volatile uint32_t *)(RecPtr->MMIOBase + (addr)) = (val);		\
   if (UMS_DEBUG&DEBUG_VERBOSE_OUTREG) {				\
      ErrorF("OUTREG(0x%lx, 0x%lx) in %s\n", (unsigned long)(addr),	\
		(unsigned long)(val), FUNCTION_NAME);			\
   }									\
} while (0)

/* To remove all debugging, make sure UMS_DEBUG is defined as a
 * preprocessor symbol, and equal to zero.
 */
#if XSERVER_LIBPCIACCESS
#define I810_MEMBASE(p,n) (p)->regions[(n)].base_addr
#define VENDOR_ID(p)      (p)->vendor_id
#define DEVICE_ID(p)      (p)->device_id
#define SUBVENDOR_ID(p)	  (p)->subvendor_id
#define SUBSYS_ID(p)      (p)->subdevice_id
#define CHIP_REVISION(p)  (p)->revision
#else
#define I810_MEMBASE(p,n) (p)->memBase[n]
#define VENDOR_ID(p)      (p)->vendor
#define DEVICE_ID(p)      (p)->chipType
#define SUBVENDOR_ID(p)	  (p)->subsysVendor
#define SUBSYS_ID(p)      (p)->subsysCard
#define CHIP_REVISION(p)  (p)->chipRev
#endif

#define I810_REG_SIZE 0x80000

/* mark chipsets for using gfx VM offset for overlay */
#define OVERLAY_NOPHYSICAL(pI810) (IS_G33CLASS(pI810) || IS_I965G(pI810))
/* mark chipsets without overlay hw */
#define OVERLAY_NOEXIST(pI810) (IS_G4X(pI810))
/* chipsets require graphics mem for hardware status page */
#define HWS_NEED_GFX(pI810) ((IS_G33CLASS(pI810) || IS_G4X(pI810)))
/* chipsets require status page in non stolen memory */
#define HWS_NEED_NONSTOLEN(pI810) (IS_G4X(pI810))
#define SUPPORTS_INTEGRATED_HDMI(pI810) (IS_G4X(pI810))
/* dsparb controlled by hw only */
#define DSPARB_HWCONTROL(pI810) (IS_G4X(pI810))

#define GTT_PAGE_SIZE			KB(4)
#define ROUND_TO(x, y)			(((x) + (y) - 1) / (y) * (y))
#define ROUND_DOWN_TO(x, y)		((x) / (y) * (y))
#define ROUND_TO_PAGE(x)		ROUND_TO((x), GTT_PAGE_SIZE)
#define ROUND_TO_MB(x)			ROUND_TO((x), MB(1))
#define PRIMARY_RINGBUFFER_SIZE		KB(128)
#define MIN_SCRATCH_BUFFER_SIZE		KB(16)
#define MAX_SCRATCH_BUFFER_SIZE		KB(64)
#define HWCURSOR_SIZE			GTT_PAGE_SIZE
#define HWCURSOR_SIZE_ARGB		GTT_PAGE_SIZE * 4
#define OVERLAY_SIZE			GTT_PAGE_SIZE

/* Use a 64x64 HW cursor */
#define I810_CURSOR_X			64
#define I810_CURSOR_Y			I810_CURSOR_X

#define PIPE_NAME(n)			('A' + (n))

#if XSERVER_LIBPCIACCESS
struct pci_device *intel_host_bridge (void);
#endif
   
#endif /* _INTEL_COMMON_H_ */
