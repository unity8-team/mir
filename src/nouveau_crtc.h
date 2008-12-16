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
 
#ifndef __NOUVEAU_CRTC_H_
#define __NOUVEAU_CRTC_H_

#include "nv_include.h"
#include "nouveau_modeset.h"

typedef struct nouveauCrtc {
	ScrnInfoPtr scrn;

	char *name;
	uint8_t index;
	Bool active;

	/* Scanout area. */
	struct nouveau_bo * front_buffer;
	uint32_t fb_pitch;
	uint32_t x; /* relative to the frontbuffer */
	uint32_t y;

	/* Gamma */
	struct nouveau_bo *lut;
	struct {
		uint16_t red;
		uint16_t green;
		uint16_t blue;
		uint16_t unused;
	} lut_values[256];
	bool lut_values_valid;

	/* Options and some state. */
	Bool modeset_lock;
	Bool dithering;
	Bool cursor_visible;
	Bool use_native_mode;
	int scale_mode;
	int pixel_clock;

	/* Mode info. */
	DisplayModePtr cur_mode;
	DisplayModePtr native_mode;
	DisplayModePtr mode_list;

	/* Function pointers. */
	Bool (*ModeValid) (nouveauCrtcPtr crtc, DisplayModePtr mode);
	void (*ModeSet) (nouveauCrtcPtr crtc, DisplayModePtr mode);
	void (*SetPixelClock) (nouveauCrtcPtr crtc, int clock);
	void (*SetClockMode) (nouveauCrtcPtr crtc, int clock); /* maybe another name? */

	void (*SetFB) (nouveauCrtcPtr crtc, struct nouveau_bo * buffer);
	void (*SetFBOffset) (nouveauCrtcPtr crtc, uint32_t x, uint32_t y);

	void (*Blank) (nouveauCrtcPtr crtc, Bool blanked);
	void (*SetDither) (nouveauCrtcPtr crtc);

	void (*SetScaleMode) (nouveauCrtcPtr crtc, int scale);

	void (*ShowCursor) (nouveauCrtcPtr crtc, Bool forced_lock);
	void (*HideCursor) (nouveauCrtcPtr crtc, Bool forced_lock);
	void (*SetCursorPosition) (nouveauCrtcPtr crtc, int x, int y);
	void (*LoadCursor) (nouveauCrtcPtr crtc, Bool argb, uint32_t *src);

	void (*GammaSet) (nouveauCrtcPtr crtc, uint16_t *red, uint16_t *green, uint16_t *blue, int size);

	void (*Save) (nouveauCrtcPtr crtc);
	void (*Load) (nouveauCrtcPtr crtc);
} nouveauCrtcRec;

#endif /* __NOUVEAU_CRTC_H_ */
