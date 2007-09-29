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
#ifndef I830_HWMC_H
#define I830_HWMC_H

#include <xf86xvmc.h>

#define XVMC_DRIVER_MPEG2_MC    0x0001
#define XVMC_DRIVER_MPEG2_VLD   0x0002
#define XVMC_DRIVER_H264_MC     0x0004
#define XVMC_DRIVER_H264_VLD    0x0008

struct intel_xvmc_driver {
    char *name;
    XF86MCAdaptorPtr adaptor;
    unsigned int flag;
    /* more items for xvmv surface manage? */
    Bool (*init)(ScrnInfoPtr, XF86VideoAdaptorPtr);
    void (*fini)(ScrnInfoPtr);
    int (*putimage_size)(ScrnInfoPtr);
    void* devPrivate;
};

extern struct intel_xvmc_driver *xvmc_driver;
extern struct intel_xvmc_driver i915_xvmc_driver;
/* extern struct intel_xvmc_driver i965_xvmc_driver; */

extern Bool intel_xvmc_set_driver(struct intel_xvmc_driver *);
extern Bool intel_xvmc_probe(ScrnInfoPtr);
extern Bool intel_xvmc_driver_init(ScreenPtr, XF86VideoAdaptorPtr);
extern Bool intel_xvmc_screen_init(ScreenPtr);
extern void intel_xvmc_finish(ScrnInfoPtr);
extern int intel_xvmc_putimage_size(ScrnInfoPtr);

#endif
