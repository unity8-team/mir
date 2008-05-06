/*
 * Copyright 2008 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv50_randr.h"
#include "X11/Xatom.h"

/*
 * A randr-1.2 wrapper around the NV50 driver.
 */

/*
 * CRTC stuff.
 */

static void
nv50_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_dpms is called with mode %d for %s.\n", mode, nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	switch (mode) {
		case DPMSModeOn:
			nv_crtc->crtc->active = TRUE;
			break;
		case DPMSModeSuspend:
		case DPMSModeStandby:
		case DPMSModeOff:
		default:
			nv_crtc->crtc->active = FALSE;
			break;
	}
}

static Bool
nv50_crtc_lock(xf86CrtcPtr crtc)
{
	return FALSE;
}

static Bool
nv50_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
nv50_crtc_prepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_prepare is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	NVPtr pNv = NVPTR(pScrn);

	nv_crtc->crtc->active = TRUE;
	nv_crtc->crtc->modeset_lock = TRUE;

	nouveauOutputPtr output;

	/* Detach any unused outputs. */
	for (output = pNv->output; output != NULL; output = output->next) {
		if (!output->crtc)
			output->ModeSet(output, NULL);
	}
}

static void
nv50_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_mode_set is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	NVPtr pNv = NVPTR(pScrn);

	/* Maybe move this elsewhere? */
	nv_crtc->crtc->SetFB(nv_crtc->crtc, pNv->FB);
	nv_crtc->crtc->SetFBOffset(nv_crtc->crtc, x, y);
	nv_crtc->crtc->ModeSet(nv_crtc->crtc, mode);
}

static void
nv50_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_gamma_set is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	nv_crtc->crtc->GammaSet(nv_crtc->crtc, (uint16_t *) red, (uint16_t *) green, (uint16_t *) blue, size);
}

static void
nv50_crtc_commit(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_commit is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	NVPtr pNv = NVPTR(pScrn);

	/* Let's detect any outputs and connectors that have gone inactive. */
	uint8_t crtc_active_mask = 0;
	int i, j;
	nouveauOutputPtr output;

	for (i = 0; i < MAX_NUM_DCB_ENTRIES; i++) {
		Bool connector_active = FALSE;
		for (j = 0; j < MAX_OUTPUTS_PER_CONNECTOR; j++) {
			output = pNv->connector[i]->outputs[j];
			if (output) {
				if (output->crtc) {
					crtc_active_mask |= 1 << output->crtc->index;
					connector_active = TRUE;
				} else {
					output->active = FALSE;
				}
			}
		}

		pNv->connector[i]->active = connector_active;
	}

	/* Blank any crtc's that are inactive. */
	if (!(crtc_active_mask & (1 << 0)))
		pNv->crtc[0]->Blank(pNv->crtc[0], TRUE);

	if (!(crtc_active_mask & (1 << 1)))
		pNv->crtc[1]->Blank(pNv->crtc[1], TRUE);

	xf86_reload_cursors(pScrn->pScreen);

	NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);

	nv_crtc->crtc->modeset_lock = FALSE;
}

/*
 * Cursor CRTC stuff.
 */

static void 
nv50_crtc_show_cursor(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_show_cursor is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	nv_crtc->crtc->ShowCursor(nv_crtc->crtc, FALSE);
}

static void
nv50_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_hide_cursor is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	nv_crtc->crtc->HideCursor(nv_crtc->crtc, FALSE);
}

static void
nv50_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;

	nv_crtc->crtc->SetCursorPosition(nv_crtc->crtc, x, y);
}

static void
nv50_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *src)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_load_cursor_argb is called for %s.\n", nv_crtc->crtc->index ? "CRTC1" : "CRTC0");

	nv_crtc->crtc->LoadCursor(nv_crtc->crtc, TRUE, (uint32_t *) src);
}

static void
nv50_crtc_destroy(xf86CrtcPtr crtc)
{
	xfree(crtc->driver_private);
}

static const xf86CrtcFuncsRec nv50_crtc_funcs = {
	.dpms = nv50_crtc_dpms,
	.save = NULL,
	.restore = NULL,
	.lock = nv50_crtc_lock,
	.unlock = NULL,
	.mode_fixup = nv50_crtc_mode_fixup,
	.prepare = nv50_crtc_prepare,
	.mode_set = nv50_crtc_mode_set,
	.gamma_set = nv50_crtc_gamma_set,
	.commit = nv50_crtc_commit,
	.shadow_create = NULL,
	.shadow_destroy = NULL,
	.set_cursor_position = nv50_crtc_set_cursor_position,
	.show_cursor = nv50_crtc_show_cursor,
	.hide_cursor = nv50_crtc_hide_cursor,
	.load_cursor_argb = nv50_crtc_load_cursor_argb,
	.destroy = nv50_crtc_destroy,
};

void
nv50_crtc_init(ScrnInfoPtr pScrn, int crtc_num)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcPtr crtc;
	NV50CrtcPrivatePtr nv_crtc;

	crtc = xf86CrtcCreate(pScrn, &nv50_crtc_funcs);
	if (crtc == NULL)
		return;

	nv_crtc = xnfcalloc (sizeof (NV50CrtcPrivateRec), 1);
	nv_crtc->crtc = pNv->crtc[crtc_num];

	crtc->driver_private = nv_crtc;
}


/*
 * "Output" stuff.
 */

static void
nv50_output_dpms(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_dpms is called with mode %d.\n", mode);

	NVPtr pNv = NVPTR(pScrn);
	NV50OutputPrivatePtr nv_output = output->driver_private;

	/* Keep the crtc wiring consistent with randr-1.2 */
	if (output->crtc) {
		NV50CrtcPrivatePtr nv_crtc = output->crtc->driver_private;
		nv_output->output->crtc = nv_crtc->crtc;
	} else {
		nv_output->output->crtc = NULL;
	}

	/* Our crtc's map 1:1 onto randr-1.2 crtc's. */
	switch (mode) {
		case DPMSModeOn:
			nv_output->output->active = TRUE;
			break;
		case DPMSModeSuspend:
		case DPMSModeStandby:
		case DPMSModeOff:
		default:
			nv_output->output->active = FALSE;
			break;
	}

	/* Set dpms on all outputs for ths connector, just to be safe. */
	nouveauConnectorPtr connector = pNv->connector[nv_output->output->dcb->bus];
	int i;
	for (i = 0; i < MAX_OUTPUTS_PER_CONNECTOR; i++) {
		if (connector->outputs[i])
			connector->outputs[i]->SetPowerMode(connector->outputs[i], mode);
	}
}

static int
nv50_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_mode_valid is called.\n");

	NV50OutputPrivatePtr nv_output = output->driver_private;

	return nv_output->output->ModeValid(nv_output->output, mode);
}

static Bool
nv50_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
nv50_output_mode_set(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_mode_set is called.\n");

	NV50OutputPrivatePtr nv_output = output->driver_private;

	nv_output->output->ModeSet(nv_output->output, mode);
}

static xf86OutputStatus
nv50_output_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_detect is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	NV50OutputPrivatePtr nv_output = output->driver_private;
	nouveauConnectorPtr connector = pNv->connector[nv_output->output->dcb->bus];

	if (!connector)
		return XF86OutputStatusDisconnected;

	Bool load_present = FALSE;
	xf86MonPtr ddc_mon = connector->DDCDetect(connector);
	int i;

	if (!ddc_mon) {
		for (i = 0; i < MAX_OUTPUTS_PER_CONNECTOR; i++) {
			if (connector->outputs[i] && connector->outputs[i]->Detect) {
				load_present = connector->outputs[i]->Detect(connector->outputs[i]);
				if (load_present)
					break;
			}
		}
	}

	/* HACK: assume that a connector only holds output in the case of a tv-out */
	if (nv_output->output->type == OUTPUT_TV)
		return XF86OutputStatusUnknown;

	/*
	 * We abuse randr-1.2 outputs as connector, so here we have to determine what actual output is connected to the connector.
	 */
	if (ddc_mon || load_present) {
		Bool is_digital = FALSE;
		Bool found = FALSE;
		nouveauCrtcPtr crtc_backup = nv_output->output->crtc;
		nv_output->output->crtc = NULL;
		nv_output->output->connector = NULL;

		if (ddc_mon)
			is_digital = ddc_mon->features.input_type;

		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Detected a %s output on %s\n", is_digital ? "Digital" : "Analog", connector->name);

		for (i = 0; i < MAX_OUTPUTS_PER_CONNECTOR; i++) {
			if (!is_digital && (connector->outputs[i]->type == OUTPUT_ANALOG || connector->outputs[i]->type == OUTPUT_TV)) {
				found = TRUE;
			} else if (is_digital && (connector->outputs[i]->type == OUTPUT_TMDS || connector->outputs[i]->type == OUTPUT_LVDS)) {
				found = TRUE;
			}
			if (found) {
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found a suitable output, index %d\n", i);
				connector->connected_output = i;
				connector->outputs[i]->connector = connector;
				connector->outputs[i]->crtc = crtc_backup;
				nv_output->output = connector->outputs[i];
				break;
			}
		}
	}

	if (ddc_mon || load_present)
		return XF86OutputStatusConnected;
	else
		return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv50_output_get_modes(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_detect is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	NV50OutputPrivatePtr nv_output = output->driver_private;
	nouveauConnectorPtr connector = pNv->connector[nv_output->output->dcb->bus];

	xf86MonPtr ddc_mon = connector->DDCDetect(connector);

	xf86OutputSetEDID(output, ddc_mon);

	DisplayModePtr ddc_modes = connector->GetDDCModes(connector);

	/* LVDS has a fixed native mode. */
	if (nv_output->output->type != OUTPUT_LVDS) {
		xf86DeleteMode(&nv_output->output->native_mode, nv_output->output->native_mode);
		nv_output->output->native_mode = NULL;
		if (nv_output->output->crtc)
			nv_output->output->crtc->native_mode = NULL;
	}

	/* NV5x hardware can also do scaling on analog connections. */
	if (nv_output->output->type != OUTPUT_LVDS && ddc_modes) {
		/* Use the first preferred mode as native mode. */
		DisplayModePtr mode;

		/* Find the preferred mode. */
		for (mode = ddc_modes; mode != NULL; mode = mode->next) {
			if (mode->type & M_T_PREFERRED) {
				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
						"%s: preferred mode is %s\n",
						output->name, mode->name);
				break;
			}
		}

		/* TODO: Scaling needs a native mode, maybe fail in a better way. */
		if (!mode) {
			mode = ddc_modes;
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"%s: no preferred mode found, using %s\n",
				output->name, mode->name);
		}

		nv_output->output->native_mode = xf86DuplicateMode(mode);
	}

	/* No ddc means no native mode, so make one up to avoid crashes. */
	if (!nv_output->output->native_mode)
		nv_output->output->native_mode = xf86CVTMode(1024, 768, 60.0, FALSE, FALSE);

	xf86SetModeCrtc(nv_output->output->native_mode, 0);

	if (nv_output->output->crtc)
			nv_output->output->crtc->native_mode =nv_output->output->native_mode;

	return ddc_modes;
}

static void
nv50_output_destroy(xf86OutputPtr output)
{
	NV50OutputPrivatePtr nv_output = output->driver_private;

	xf86DeleteMode(&nv_output->output->native_mode, nv_output->output->native_mode);

	xfree(output->driver_private);
	output->driver_private = NULL;
}

void
nv50_output_prepare(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_prepare is called.\n");

	NV50OutputPrivatePtr nv_output = output->driver_private;
	NV50CrtcPrivatePtr nv_crtc = output->crtc->driver_private;

	/* Set the real crtc now. */
	nv_output->output->crtc = nv_crtc->crtc;

	/* Transfer some output properties to the crtc for easy access. */
	nv_output->output->crtc->scale_mode = nv_output->output->scale_mode;
	nv_output->output->crtc->dithering = nv_output->output->dithering;
	nv_output->output->crtc->native_mode = nv_output->output->native_mode;

	if (nv_output->output->scale_mode != SCALE_PANEL)
		nv_output->output->crtc->use_native_mode = TRUE;
	else
		nv_output->output->crtc->use_native_mode = FALSE;
}

static void
nv50_output_commit(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_output_commit is called.\n");
}

/*
 * Output properties.
 */

/*
 * Several scaling modes exist, let the user choose.
 */
#define SCALING_MODE_NAME "SCALING_MODE"
static const struct {
	char *name;
	enum scaling_modes mode;
} scaling_mode[] = {
	{ "panel", SCALE_PANEL },
	{ "fullscreen", SCALE_FULLSCREEN },
	{ "aspect", SCALE_ASPECT },
	{ "noscale", SCALE_NOSCALE },
	{ NULL, SCALE_INVALID}
};
static Atom scaling_mode_atom;

#define DITHERING_MODE_NAME "DITHERING"
static Atom dithering_atom;

int
nv_scaling_mode_lookup(char *name, int size)
{
	int i;

	/* for when name is zero terminated */
	if (size < 0)
		size = strlen(name);

	for (i = 0; scaling_mode[i].name; i++)
		/* We're getting non-terminated strings */
		if (strlen(scaling_mode[i].name) >= size &&
				!strncasecmp(name, scaling_mode[i].name, size))
			break;

	return scaling_mode[i].mode;
}

void
nv50_output_create_resources(xf86OutputPtr output)
{
	NV50OutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	INT32 dithering_range[2] = { 0, 1 };
	int error, i;

	/*
	 * Setup scaling mode property.
	 */
	scaling_mode_atom = MakeAtom(SCALING_MODE_NAME, sizeof(SCALING_MODE_NAME) - 1, TRUE);

	error = RRConfigureOutputProperty(output->randr_output,
					scaling_mode_atom, TRUE, FALSE, FALSE,
					0, NULL);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"RRConfigureOutputProperty error, %d\n", error);
	}

	char *existing_scale_name = NULL;
	for (i = 0; scaling_mode[i].name; i++)
		if (scaling_mode[i].mode == nv_output->output->scale_mode)
			existing_scale_name = scaling_mode[i].name;

	error = RRChangeOutputProperty(output->randr_output, scaling_mode_atom,
					XA_STRING, 8, PropModeReplace, 
					strlen(existing_scale_name),
					existing_scale_name, FALSE, TRUE);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to set scaling mode, %d\n", error);
	}

	if (nv_output->output->type == OUTPUT_TMDS || nv_output->output->type == OUTPUT_LVDS) {
		/*
		 * Setup dithering property.
		 */
		dithering_atom = MakeAtom(DITHERING_MODE_NAME, sizeof(DITHERING_MODE_NAME) - 1, TRUE);

		error = RRConfigureOutputProperty(output->randr_output,
						dithering_atom, TRUE, TRUE, FALSE,
						2, dithering_range);

		if (error != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"RRConfigureOutputProperty error, %d\n", error);
		}

		error = RRChangeOutputProperty(output->randr_output, dithering_atom,
						XA_INTEGER, 32, PropModeReplace, 1, &nv_output->output->dithering,
						FALSE, TRUE);

		if (error != 0) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Failed to set dithering mode, %d\n", error);
		}
	}
}

Bool
nv50_output_set_property(xf86OutputPtr output, Atom property,
				RRPropertyValuePtr value)
{
	NV50OutputPrivatePtr nv_output = output->driver_private;

	if (property == scaling_mode_atom) {
		int32_t ret;
		char *name = NULL;

		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		name = (char *) value->data;

		/* Match a string to a scaling mode */
		ret = nv_scaling_mode_lookup(name, value->size);
		if (ret == SCALE_INVALID)
			return FALSE;

		/* LVDS must always use gpu scaling. */
		if (ret == SCALE_PANEL && nv_output->output->type == OUTPUT_LVDS)
			return FALSE;

		nv_output->output->scale_mode = ret;
		if (nv_output->output->crtc) /* normally prepare sets all these things for the crtc. */
			nv_output->output->crtc->scale_mode = ret;
		return TRUE;
	} else if (property == dithering_atom) {
		if (value->type != XA_INTEGER || value->format != 32)
			return FALSE;

		int32_t val = *(int32_t *) value->data;

		if (val < 0 || val > 1)
			return FALSE;

		nv_output->output->dithering = val;
		if (nv_output->output->crtc) /* normally prepare sets all these things for the crtc. */
			nv_output->output->crtc->dithering = val;
		return TRUE;
	}

	return TRUE;
}

static const xf86OutputFuncsRec nv50_output_funcs = {
	.dpms = nv50_output_dpms,
	.save = NULL,
	.restore = NULL,
	.mode_valid = nv50_output_mode_valid,
	.mode_fixup = nv50_output_mode_fixup,
	.mode_set = nv50_output_mode_set,
	.detect = nv50_output_detect,
	.get_modes = nv50_output_get_modes,
	.destroy = nv50_output_destroy,
	.prepare = nv50_output_prepare,
	.commit = nv50_output_commit,
	.create_resources = nv50_output_create_resources,
	.set_property = nv50_output_set_property,
};

void
nv50_output_create(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr output;
	NV50OutputPrivatePtr nv_output;
	int i;

	/* this is a 1:1 hookup of the connectors. */
	for (i = 0; i < MAX_NUM_DCB_ENTRIES; i++) {
		if (!(pNv->connector[i]->outputs[0]))
			continue; /* An empty connector is not useful. */

		if (!(output = xf86OutputCreate(pScrn, &nv50_output_funcs, pNv->connector[i]->name)))
			return;

		if (!(nv_output = xnfcalloc(sizeof(NV50OutputPrivateRec), 1)))
			return;

		output->driver_private = nv_output;

		nv_output->output = pNv->connector[i]->outputs[0]; /* initially just wire up the first output available. */

		output->possible_crtcs = nv_output->output->allowed_crtc;
		output->possible_clones = 0;
	}
}
