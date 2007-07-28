/*
 * Copyright © 2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Zhenyu Wang <zhenyu.z.wang@intel.com>
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "i830.h"

#define SUBSYS_ANY (~0)

typedef struct {
    int chipType;
    int subsysVendor;
    int subsysCard;
    void (*hook)(I830Ptr);
} i830_quirk, *i830_quirk_ptr;

static void quirk_ignore_tv (I830Ptr pI830)
{
    pI830->quirk_flag |= QUIRK_IGNORE_TV;
}

static void quirk_ignore_lvds (I830Ptr pI830)
{
    pI830->quirk_flag |= QUIRK_IGNORE_LVDS;
}

static void quirk_mac_mini (I830Ptr pI830)
{
    pI830->quirk_flag |= QUIRK_IGNORE_MACMINI_LVDS;
}

static i830_quirk i830_quirk_list[] = {
    /* Lenovo T61 has no TV output */
    { PCI_CHIP_I965_GM, 0x17aa, 0x20b5, quirk_ignore_tv },
    /* Lenovo 3000 v200 */
    { PCI_CHIP_I965_GM, 0x17aa, 0x3c18, quirk_ignore_tv },
    /* Aopen mini pc */
    { PCI_CHIP_I945_GM, 0xa0a0, SUBSYS_ANY, quirk_ignore_lvds },
    /* Mac mini has no lvds, but macbook pro does */
    { PCI_CHIP_I945_GM, 0x8086, 0x7270, quirk_mac_mini },
    { 0, 0, 0, NULL },
};

void i830_fixup_devices(ScrnInfoPtr scrn)
{
    I830Ptr pI830 = I830PTR(scrn);
    i830_quirk_ptr p = i830_quirk_list;

    while (p && p->chipType != 0) {
	if (pI830->PciInfo->chipType == p->chipType &&
		pI830->PciInfo->subsysVendor == p->subsysVendor &&
		(pI830->PciInfo->subsysCard == p->subsysCard ||
		 p->subsysCard == SUBSYS_ANY))
	    p->hook(pI830);
	++p;
    }
}
