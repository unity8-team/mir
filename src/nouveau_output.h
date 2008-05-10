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

#ifndef __NOUVEAU_OUTPUT_H_
#define __NOUVEAU_OUTPUT_H_

#include "nv_include.h"
#include "nouveau_modeset.h"
#include "nouveau_crtc.h"
#include "nouveau_connector.h"

typedef struct nouveauOutput {
	ScrnInfoPtr scrn;

	char *name;
	Bool active;
	nouveauOutputPtr next;

	nouveauCrtcPtr crtc;
	nouveauConnectorPtr connector; /* the one that is currently in use, not all possibilities. */

	/* This can change in rare circumstances, when an output resource is shared. */
	struct dcb_entry *dcb;
	int type;
	uint8_t allowed_crtc; /* bit0: crtc0, bit1: crtc1 */
	int scale_mode;
	Bool dithering;

	/* Mode stuff. */
	DisplayModePtr native_mode;

	/* Function pointers. */
	int (*ModeValid) (nouveauOutputPtr output, DisplayModePtr mode);
	void (*ModeSet) (nouveauOutputPtr output, DisplayModePtr mode);
	void (*SetClockMode) (nouveauOutputPtr output, int clock); /* maybe another name? */

	/* This will handle the case where output resources are shared. */
	int (*Sense) (nouveauOutputPtr output); /* this is not for ddc or load detect, and will often just return a fixed type. */
	Bool (*Detect) (nouveauOutputPtr output); /* everything that isn't hotplug detect or ddc */
	DisplayModePtr (*GetFixedMode) (nouveauOutputPtr output); /* only lvds as far as i know. */

	void (*SetPowerMode) (nouveauOutputPtr output, int mode);

	void (*Save) (nouveauOutputPtr output);
	void (*Load) (nouveauOutputPtr output);
} nouveauOutputRec;

#endif /* __NOUVEAU_OUTPUT_H_ */
