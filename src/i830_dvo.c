/**************************************************************************

Copyright 2006 Dave Airlie <airlied@linux.ie>

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"

#include "sil164/sil164.h"
#include "ch7xxx/ch7xxx.h"

static const char *SIL164Symbols[] = {
    "Sil164VidOutput",
    NULL
};
static const char *CH7xxxSymbols[] = {
    "CH7xxxVidOutput",
    NULL
};

/* driver list */
struct _I830DVODriver i830_dvo_drivers[] =
{
	{ I830_DVO_CHIP_TMDS, "sil164", "SIL164VidOutput",
		(SIL164_ADDR_1<<1), SIL164Symbols, NULL , NULL, NULL},
	{ I830_DVO_CHIP_TMDS | I830_DVO_CHIP_TVOUT, "ch7xxx", "CH7xxxVidOutput",
		(CH7xxx_ADDR_1<<1), CH7xxxSymbols, NULL , NULL, NULL}
};

#define I830_NUM_DVO_DRIVERS (sizeof(i830_dvo_drivers)/sizeof(struct _I830DVODriver))

Bool
I830I2CDetectDVOControllers(ScrnInfoPtr pScrn, I2CBusPtr pI2CBus,
			    struct _I830DVODriver **retdrv)
{
    int i;
    void *ret_ptr;
    struct _I830DVODriver *drv;

    for (i = 0; i < I830_NUM_DVO_DRIVERS; i++) {
	drv = &i830_dvo_drivers[i];
	drv->modhandle = xf86LoadSubModule(pScrn, drv->modulename);
	if (drv->modhandle == NULL)
	    continue;

	xf86LoaderReqSymLists(drv->symbols, NULL);

	ret_ptr = NULL;
	drv->vid_rec = LoaderSymbol(drv->fntablename);
	if (drv->vid_rec != NULL)
	    ret_ptr = drv->vid_rec->Detect(pI2CBus, drv->address);

	if (ret_ptr != NULL) {
	    drv->dev_priv = ret_ptr;
	    *retdrv = drv;
	    return TRUE;
	}
	xf86UnloadSubModule(drv->modhandle);
    }
    return FALSE;
}
