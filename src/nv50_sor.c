/*
 * Copyright (c) 2007 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_RANDR12

#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "nv_include.h"
#include "nv50_type.h"
#include "nv50_display.h"
#include "nv50_output.h"

static void
NV50SorSetPClk(xf86OutputPtr output, int pclk)
{
    NVPtr pNv = NVPTR(output->scrn);
    NV50OutputPrivPtr pPriv = output->driver_private;
    const int orOff = 0x800 * pPriv->or;

    pNv->REGS[(0x00614300+orOff)/4] = (pclk > 165000) ? 0x101 : 0;
}

static void
NV50SorDPMSSet(xf86OutputPtr output, int mode)
{
    NVPtr pNv = NVPTR(output->scrn);
    NV50OutputPrivPtr pPriv = output->driver_private;
    const int off = 0x800 * pPriv->or;
    CARD32 tmp;

    while(pNv->REGS[(0x0061C004+off)/4] & 0x80000000);

    tmp = pNv->REGS[(0x0061C004+off)/4];
    tmp |= 0x80000000;

    if(mode == DPMSModeOn)
        tmp |= 1;
    else
        tmp &= ~1;

    pNv->REGS[(0x0061C004+off)/4] = tmp;
}

static void
NV50SorModeSet(xf86OutputPtr output, DisplayModePtr mode,
              DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    NV50OutputPrivPtr pPriv = output->driver_private;
    const int sorOff = 0x40 * pPriv->or;

    if(!adjusted_mode) {
        /* Disconnect the SOR */
        C(0x00000600 + sorOff, 0);
        return;
    }

    // This wouldn't be necessary, but the server is stupid and calls
    // NV50SorDPMSSet after the output is disconnected, even though the hardware
    // turns it off automatically.
    NV50SorDPMSSet(output, DPMSModeOn);

    C(0x00000600 + sorOff,
        (NV50CrtcGetHead(output->crtc) == HEAD0 ? 1 : 2) |
        (adjusted_mode->Clock > 165000 ? 0x500 : 0x100) |
        ((adjusted_mode->Flags & V_NHSYNC) ? 0x1000 : 0) |
        ((adjusted_mode->Flags & V_NVSYNC) ? 0x2000 : 0));
}

static xf86OutputStatus
NV50SorDetect(xf86OutputPtr output)
{

    NV50OutputPrivPtr pPriv = output->driver_private;

    /* Assume physical status isn't going to change before the BlockHandler */
    if(pPriv->cached_status != XF86OutputStatusUnknown)
        return pPriv->cached_status;

    NV50OutputPartnersDetect(pPriv->partner, output, pPriv->i2c);
    return pPriv->cached_status;
}

static void
NV50SorDestroy(xf86OutputPtr output)
{
    NV50OutputDestroy(output);

    xfree(output->driver_private);
    output->driver_private = NULL;
}

static const xf86OutputFuncsRec NV50SorOutputFuncs = {
    .dpms = NV50SorDPMSSet,
    .save = NULL,
    .restore = NULL,
    .mode_valid = NV50OutputModeValid,
    .mode_fixup = NV50OutputModeFixup,
    .prepare = NV50OutputPrepare,
    .commit = NV50OutputCommit,
    .mode_set = NV50SorModeSet,
    .detect = NV50SorDetect,
    .get_modes = NV50OutputGetDDCModes,
    .destroy = NV50SorDestroy,
};

xf86OutputPtr
NV50CreateSor(ScrnInfoPtr pScrn, ORNum or)
{
    NV50OutputPrivPtr pPriv = xnfcalloc(sizeof(*pPriv), 1);
    xf86OutputPtr output;
    char orName[5];

    if(!pPriv)
        return FALSE;

    snprintf(orName, 5, "DVI%i", or);
    output = xf86OutputCreate(pScrn, &NV50SorOutputFuncs, orName);

    pPriv->type = SOR;
    pPriv->or = or;
    pPriv->cached_status = XF86OutputStatusUnknown;
    pPriv->set_pclk = NV50SorSetPClk;
    output->driver_private = pPriv;
    output->interlaceAllowed = TRUE;
    output->doubleScanAllowed = TRUE;

    return output;
}

#endif /* ENABLE_RANDR12 */
