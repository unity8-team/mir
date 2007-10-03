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
#include <X11/Xatom.h>

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
    const int limit = 165000;

    pNv->REGS[(0x00614300+orOff)/4] = (pclk > limit) ? 0x101 : 0;
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
    while((pNv->REGS[(0x0061c030+off)/4] & 0x10000000));
}

static int
NV50TMDSModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
    // Disable dual-link modes until I can find a way to make them work
    // reliably.
    if (mode->Clock > 165000)
        return MODE_CLOCK_HIGH;

    return NV50OutputModeValid(output, mode);
}

static int
NV50LVDSModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
    NV50OutputPrivPtr pPriv = output->driver_private;
    DisplayModePtr native = pPriv->nativeMode;

    // Ignore modes larger than the native res.
    if (mode->HDisplay > native->HDisplay || mode->VDisplay > native->VDisplay)
        return MODE_PANEL;

    return NV50OutputModeValid(output, mode);
}

static void
NV50SorModeSet(xf86OutputPtr output, DisplayModePtr mode,
              DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    NV50OutputPrivPtr pPriv = output->driver_private;
    const int sorOff = 0x40 * pPriv->or;
    CARD32 type;

    if(!adjusted_mode) {
        /* Disconnect the SOR */
	NV50DisplayCommand(pScrn, 0x600 + sorOff, 0);
        return;
    }

    if (pPriv->panelType == LVDS) {
	type = 0;
    } else
    if (adjusted_mode->Clock > 165000) {
	type = 0x500;
    } else {
	type = 0x100;
    }

    // This wouldn't be necessary, but the server is stupid and calls
    // NV50SorDPMSSet after the output is disconnected, even though the hardware
    // turns it off automatically.
    NV50SorDPMSSet(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, 0x600 + sorOff,
		(NV50CrtcGetHead(output->crtc) == HEAD0 ? 1 : 2) | type |
		((adjusted_mode->Flags & V_NHSYNC) ? 0x1000 : 0) |
		((adjusted_mode->Flags & V_NVSYNC) ? 0x2000 : 0));

    NV50CrtcSetScale(output->crtc, adjusted_mode, pPriv->scale);
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

static xf86OutputStatus
NV50SorLVDSDetect(xf86OutputPtr output)
{
    /* Assume LVDS is always connected */
    return XF86OutputStatusConnected;
}

static void
NV50SorDestroy(xf86OutputPtr output)
{
    NV50OutputPrivPtr pPriv = output->driver_private;

    NV50OutputDestroy(output);

    xf86DeleteMode(&pPriv->nativeMode, pPriv->nativeMode);

    xfree(output->driver_private);
    output->driver_private = NULL;
}

static void
NV50SorSetModeBackend(DisplayModePtr dst, const DisplayModePtr src)
{
    // Stash the backend mode timings from src into dst
    dst->Clock           = src->Clock;
    dst->Flags           = src->Flags;
    dst->CrtcHDisplay    = src->CrtcHDisplay;
    dst->CrtcHBlankStart = src->CrtcHBlankStart;
    dst->CrtcHSyncStart  = src->CrtcHSyncStart;
    dst->CrtcHSyncEnd    = src->CrtcHSyncEnd;
    dst->CrtcHBlankEnd   = src->CrtcHBlankEnd;
    dst->CrtcHTotal      = src->CrtcHTotal;
    dst->CrtcHSkew       = src->CrtcHSkew;
    dst->CrtcVDisplay    = src->CrtcVDisplay;
    dst->CrtcVBlankStart = src->CrtcVBlankStart;
    dst->CrtcVSyncStart  = src->CrtcVSyncStart;
    dst->CrtcVSyncEnd    = src->CrtcVSyncEnd;
    dst->CrtcVBlankEnd   = src->CrtcVBlankEnd;
    dst->CrtcVTotal      = src->CrtcVTotal;
    dst->CrtcHAdjusted   = src->CrtcHAdjusted;
    dst->CrtcVAdjusted   = src->CrtcVAdjusted;
}

static Bool
NV50SorModeFixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
    NV50OutputPrivPtr pPriv = output->driver_private;
    DisplayModePtr native = pPriv->nativeMode;

    if(native && pPriv->scale != NV50_SCALE_OFF) {
        NV50SorSetModeBackend(adjusted_mode, native);
        // This mode is already "fixed"
        NV50CrtcSkipModeFixup(output->crtc);
    }

    return TRUE;
}

static Bool
NV50SorTMDSModeFixup(xf86OutputPtr output, DisplayModePtr mode,
                    DisplayModePtr adjusted_mode)
{
    int scrnIndex = output->scrn->scrnIndex;
    NV50OutputPrivPtr pPriv = output->driver_private;
    DisplayModePtr modes = output->probed_modes;

    xf86DeleteMode(&pPriv->nativeMode, pPriv->nativeMode);

    if(modes) {
        // Find the preferred mode and use that as the "native" mode.
        // If no preferred mode is available, use the first one.
        DisplayModePtr mode;

        // Find the preferred mode.
        for(mode = modes; mode; mode = mode->next) {
            if(mode->type & M_T_PREFERRED) {
                xf86DrvMsgVerb(scrnIndex, X_INFO, 5,
                               "%s: preferred mode is %s\n",
                               output->name, mode->name);
                break;
            }
        }

        // XXX: May not want to allow scaling if no preferred mode is found.
        if(!mode) {
            mode = modes;
            xf86DrvMsgVerb(scrnIndex, X_INFO, 5,
                    "%s: no preferred mode found, using %s\n",
                    output->name, mode->name);
        }

        pPriv->nativeMode = xf86DuplicateMode(mode);
        NV50CrtcDoModeFixup(pPriv->nativeMode, mode);
    }

    return NV50SorModeFixup(output, mode, adjusted_mode);
}

static DisplayModePtr
NV50SorGetLVDSModes(xf86OutputPtr output)
{
    NV50OutputPrivPtr pPriv = output->driver_private;
    return xf86DuplicateMode(pPriv->nativeMode);
}

#ifdef RANDR_12_INTERFACE
#define MAKE_ATOM(a) MakeAtom((a), sizeof(a) - 1, TRUE);

struct property {
    Atom atom;
    INT32 range[2];
};

static struct {
    struct property dither;
    struct property scale;
} properties;

static void
NV50SorCreateResources(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);
    int data, err;
    const char *s;

    /******** dithering ********/
    properties.dither.atom = MAKE_ATOM("dither");
    properties.dither.range[0] = 0;
    properties.dither.range[1] = 1;
    err = RRConfigureOutputProperty(output->randr_output,
                                    properties.dither.atom, FALSE, TRUE, FALSE,
                                    2, properties.dither.range);
    if(err)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to configure dithering property for %s: error %d\n",
                   output->name, err);

    // Set the default value
    data = pNv->FPDither;
    err = RRChangeOutputProperty(output->randr_output, properties.dither.atom,
                                 XA_INTEGER, 32, PropModeReplace, 1, &data,
                                 FALSE, FALSE);
    if(err)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to set dithering property for %s: error %d\n",
                   output->name, err);

    /******** scaling ********/
    properties.scale.atom = MAKE_ATOM("scale");
    err = RRConfigureOutputProperty(output->randr_output,
                                    properties.scale.atom, FALSE, FALSE,
                                    FALSE, 0, NULL);
    if(err)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to configure scaling property for %s: error %d\n",
                   output->name, err);

    // Set the default value
    s = "aspect";
    err = RRChangeOutputProperty(output->randr_output, properties.scale.atom,
                                 XA_STRING, 8, PropModeReplace, strlen(s),
                                 (pointer)s, FALSE, FALSE);
    if(err)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to set scaling property for %s: error %d\n",
                   output->name, err);
}

static Bool
NV50SorSetProperty(xf86OutputPtr output, Atom prop, RRPropertyValuePtr val)
{
    NV50OutputPrivPtr pPriv = output->driver_private;

    if(prop == properties.dither.atom) {
        INT32 i;

        if(val->type != XA_INTEGER || val->format != 32 || val->size != 1)
            return FALSE;

        i = *(INT32*)val->data;
        if(i < properties.dither.range[0] || i > properties.dither.range[1])
            return FALSE;

        NV50CrtcSetDither(output->crtc, i, TRUE);
        return TRUE;
    } else if(prop == properties.scale.atom) {
        const char *s;
        enum NV50ScaleMode oldScale, scale;
        int i;
        const struct {
            const char *name;
            enum NV50ScaleMode scale;
        } modes[] = {
            { "off",    NV50_SCALE_OFF },
            { "aspect", NV50_SCALE_ASPECT },
            { "fill",   NV50_SCALE_FILL },
            { "center", NV50_SCALE_CENTER },
            { NULL,     0 },
        };

        if(val->type != XA_STRING || val->format != 8)
            return FALSE;
        s = (char*)val->data;

        for(i = 0; modes[i].name; i++) {
            const char *name = modes[i].name;
            const int len = strlen(name);

            if(val->size == len && !strncmp(name, s, len)) {
                scale = modes[i].scale;
                break;
            }
        }
        if(!modes[i].name)
            return FALSE;
        if(scale == NV50_SCALE_OFF && pPriv->panelType == LVDS)
            // LVDS requires scaling
            return FALSE;

        oldScale = pPriv->scale;
        pPriv->scale = scale;
        if(output->crtc) {
            xf86CrtcPtr crtc = output->crtc;

            if(!xf86CrtcSetMode(crtc, &crtc->desiredMode, crtc->desiredRotation,
                                crtc->desiredX, crtc->desiredY)) {
                xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
                           "Failed to set scaling to %s for output %s\n",
                           modes[i].name, output->name);

                // Restore old scale and try again.
                pPriv->scale = oldScale;
                if(!xf86CrtcSetMode(crtc, &crtc->desiredMode,
                                    crtc->desiredRotation, crtc->desiredX,
                                    crtc->desiredY)) {
                    xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
                               "Failed to restore old scaling for output %s\n",
                               output->name);
                }

                return FALSE;
            }
        }
        return TRUE;
    }

    return FALSE;
}
#endif // RANDR_12_INTERFACE

static const xf86OutputFuncsRec NV50SorTMDSOutputFuncs = {
    .dpms = NV50SorDPMSSet,
    .save = NULL,
    .restore = NULL,
    .mode_valid = NV50TMDSModeValid,
    .mode_fixup = NV50SorTMDSModeFixup,
    .prepare = NV50OutputPrepare,
    .commit = NV50OutputCommit,
    .mode_set = NV50SorModeSet,
    .detect = NV50SorDetect,
    .get_modes = NV50OutputGetDDCModes,
#ifdef RANDR_12_INTERFACE
    .create_resources = NV50SorCreateResources,
    .set_property = NV50SorSetProperty,
#endif
    .destroy = NV50SorDestroy,
};

static const xf86OutputFuncsRec NV50SorLVDSOutputFuncs = {
    .dpms = NV50SorDPMSSet,
    .save = NULL,
    .restore = NULL,
    .mode_valid = NV50LVDSModeValid,
    .mode_fixup = NV50SorModeFixup,
    .prepare = NV50OutputPrepare,
    .commit = NV50OutputCommit,
    .mode_set = NV50SorModeSet,
    .detect = NV50SorLVDSDetect,
    .get_modes = NV50SorGetLVDSModes,
#ifdef RANDR_12_INTERFACE
    .create_resources = NV50SorCreateResources,
    .set_property = NV50SorSetProperty,
#endif
    .destroy = NV50SorDestroy,
};

static DisplayModePtr
GetLVDSNativeMode(NVPtr pNv)
{
    DisplayModePtr mode = xnfcalloc(1, sizeof(DisplayModeRec));
    const CARD32 size = pNv->REGS[0x00610B4C/4];
    const int width = size & 0x3fff;
    const int height = (size >> 16) & 0x3fff;

    mode->HDisplay = mode->CrtcHDisplay = width;
    mode->VDisplay = mode->CrtcVDisplay = height;
    mode->Clock           = pNv->REGS[0x610AD4/4] & 0x3fffff;
    mode->CrtcHBlankStart = pNv->REGS[0x610AFC/4];
    mode->CrtcHSyncEnd    = pNv->REGS[0x610B04/4];
    mode->CrtcHBlankEnd   = pNv->REGS[0x610AE8/4];
    mode->CrtcHTotal      = pNv->REGS[0x610AF4/4];

    mode->next = mode->prev = NULL;
    mode->status = MODE_OK;
    mode->type = M_T_DRIVER | M_T_PREFERRED;

    xf86SetModeDefaultName(mode);

    return mode;
}

xf86OutputPtr
NV50CreateSor(ScrnInfoPtr pScrn, ORNum or, PanelType panelType)
{
    NVPtr pNv = NVPTR(pScrn);
    NV50OutputPrivPtr pPriv = xnfcalloc(sizeof(*pPriv), 1);
    const int off = 0x800 * or;
    xf86OutputPtr output;
    char orName[5];
    const xf86OutputFuncsRec *funcs;

    if(!pPriv)
        return FALSE;

    if(panelType == LVDS) {
        strcpy(orName, "LVDS");
        funcs = &NV50SorLVDSOutputFuncs;
    } else {
        snprintf(orName, 5, "DVI%d", or);
        pNv->REGS[(0x61C00C+off)/4] = 0x03010700;
        pNv->REGS[(0x61C010+off)/4] = 0x0000152f;
        pNv->REGS[(0x61C014+off)/4] = 0x00000000;
        pNv->REGS[(0x61C018+off)/4] = 0x00245af8;
        funcs = &NV50SorTMDSOutputFuncs;
    }

    output = xf86OutputCreate(pScrn, funcs, orName);

    pPriv->type = SOR;
    pPriv->or = or;
    pPriv->panelType = panelType;
    pPriv->cached_status = XF86OutputStatusUnknown;
    if (panelType == TMDS)
	pPriv->set_pclk = NV50SorSetPClk;
    output->driver_private = pPriv;
    output->interlaceAllowed = TRUE;
    output->doubleScanAllowed = TRUE;

    if(panelType == LVDS) {
        pPriv->nativeMode = GetLVDSNativeMode(pNv);

        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s native size %dx%d\n",
                   orName, pPriv->nativeMode->HDisplay,
                   pPriv->nativeMode->VDisplay);
    }

    return output;
}

#endif /* ENABLE_RANDR12 */
