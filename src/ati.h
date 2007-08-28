/*
 * Copyright 1999 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ___ATI_H___
#define ___ATI_H___ 1

#include <unistd.h>
#include "xf86Pci.h"
#include "atipciids.h"

#include "xf86.h"

#include "xf86_OSproc.h"

extern DriverRec ATI;

/*
 * Chip-related definitions.
 */
typedef enum
{
    ATI_CHIP_NONE = 0,
    ATI_CHIP_88800GXC,          /* Mach64 */
    ATI_CHIP_88800GXD,          /* Mach64 */
    ATI_CHIP_88800GXE,          /* Mach64 */
    ATI_CHIP_88800GXF,          /* Mach64 */
    ATI_CHIP_88800GX,           /* Mach64 */
    ATI_CHIP_88800CX,           /* Mach64 */
    ATI_CHIP_264CT,             /* Mach64 */
    ATI_CHIP_264ET,             /* Mach64 */
    ATI_CHIP_264VT,             /* Mach64 */
    ATI_CHIP_264GT,             /* Mach64 */
    ATI_CHIP_264VTB,            /* Mach64 */
    ATI_CHIP_264GTB,            /* Mach64 */
    ATI_CHIP_264VT3,            /* Mach64 */
    ATI_CHIP_264GTDVD,          /* Mach64 */
    ATI_CHIP_264LT,             /* Mach64 */
    ATI_CHIP_264VT4,            /* Mach64 */
    ATI_CHIP_264GT2C,           /* Mach64 */
    ATI_CHIP_264GTPRO,          /* Mach64 */
    ATI_CHIP_264LTPRO,          /* Mach64 */
    ATI_CHIP_264XL,             /* Mach64 */
    ATI_CHIP_MOBILITY,          /* Mach64 */
    ATI_CHIP_Mach64,            /* Last among Mach64's */
    ATI_CHIP_RAGE128GL,         /* Rage128 */
    ATI_CHIP_RAGE128VR,         /* Rage128 */
    ATI_CHIP_RAGE128PROGL,      /* Rage128 */
    ATI_CHIP_RAGE128PROVR,      /* Rage128 */
    ATI_CHIP_RAGE128PROULTRA,   /* Rage128 */
    ATI_CHIP_RAGE128MOBILITY3,  /* Rage128 */
    ATI_CHIP_RAGE128MOBILITY4,  /* Rage128 */
    ATI_CHIP_Rage128,           /* Last among Rage128's */
    ATI_CHIP_RADEON,            /* Radeon */
    ATI_CHIP_RADEONVE,          /* Radeon VE */
    ATI_CHIP_RADEONMOBILITY6,   /* Radeon M6 */
    ATI_CHIP_RS100,             /* IGP320 */
    ATI_CHIP_RS200,             /* IGP340 */
    ATI_CHIP_RS250,             /* Radoen 7000 IGP */
    ATI_CHIP_RV200,             /* RV200 */
    ATI_CHIP_RADEONMOBILITY7,   /* Radeon M7 */
    ATI_CHIP_R200,              /* R200 */
    ATI_CHIP_RV250,             /* RV250 */
    ATI_CHIP_RADEONMOBILITY9,   /* Radeon M9 */
    ATI_CHIP_RS300,             /* Radoen 9100 IGP */
    ATI_CHIP_RS350,             /* Radoen 9200 IGP */
    ATI_CHIP_RV280,             /* RV250 */
    ATI_CHIP_RADEONMOBILITY9PLUS,   /* Radeon M9+ */
    ATI_CHIP_R300,              /* R300 */
    ATI_CHIP_RV350,             /* RV350/M10/M11 */
    ATI_CHIP_R350,              /* R350 */
    ATI_CHIP_R360,              /* R360 */
    ATI_CHIP_RV370,             /* RV370/M22 */
    ATI_CHIP_RV380,             /* RV380/M24 */
    ATI_CHIP_R420,              /* R420/M18 */
    ATI_CHIP_R423,              /* R423/M28? */
    ATI_CHIP_R430,              /* R430 */
    ATI_CHIP_R480,              /* R480/M28? */
    ATI_CHIP_R481,              /* R481 */
    ATI_CHIP_RV410,             /* RV410, M26 */
    ATI_CHIP_RS400,             /* RS400, RS410, RS480, RS482, ... */
    ATI_CHIP_Radeon,            /* Last among Radeon's */
    ATI_CHIP_HDTV               /* HDTV */
} ATIChipType;

extern const char *ATIChipNames[];

extern ATIChipType ATIChipID(const CARD16, const CARD8);

#endif /* ___ATI_H___ */
