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

#ifndef __NOUVEAU_CONNECTOR_H_
#define __NOUVEAU_CONNECTOR_H_

#include "nv_include.h"
#include "nouveau_modeset.h"
#include "nouveau_crtc.h"
#include "nouveau_output.h"

/* I have yet to find specific information on connectors, so it's all derived from outputs. */
typedef enum {
	CONNECTOR_NONE = 4,
	CONNECTOR_VGA = 0,
	CONNECTOR_DVI = 1,
	CONNECTOR_TV = 2,
	CONNECTOR_PANEL = 3
} NVConnectorType;

#define MAX_OUTPUTS_PER_CONNECTOR 2

typedef struct nouveauConnector {
	ScrnInfoPtr scrn;
	int index;

	char *name;
	Bool active;

	NVConnectorType type;

	I2CBusPtr pDDCBus;
	int i2c_index;

	/* For load detect amongst other things. */
	nouveauOutputPtr outputs[MAX_OUTPUTS_PER_CONNECTOR];
	int connected_output;

	Bool hotplug_detected; /* better name? */
	/* Function pointers. */
	Bool (*HotplugDetect) (nouveauConnectorPtr connector);
	xf86MonPtr (*DDCDetect) (nouveauConnectorPtr connector);
	DisplayModePtr (*GetDDCModes) (nouveauConnectorPtr connector);
} nouveauConnectorRec;

#endif /* __NOUVEAU_CONNECTOR_H_ */
