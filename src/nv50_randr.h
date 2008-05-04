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

#ifndef __NV50_RANDR_H_
#define __NV50_RANDR_H_

#include "nouveau_modeset.h"
#include "nouveau_crtc.h"
#include "nouveau_output.h"
#include "nouveau_connector.h"

typedef struct NV50CrtcPrivate {
	int head;
	nouveauCrtcPtr crtc;
} NV50CrtcPrivateRec, *NV50CrtcPrivatePtr;

typedef struct NV50OutputPrivate {
	nouveauOutputPtr output;
} NV50OutputPrivateRec, *NV50OutputPrivatePtr;

#endif /* __NV50_RANDR_H_ */
