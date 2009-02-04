/*
 * Copyright 2007 Nouveau Project
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

#ifndef __NOUVEAU_LOCAL_H__
#define __NOUVEAU_LOCAL_H__

#include "compiler.h"
#include "xf86_OSproc.h"

/* Debug output */
#define NOUVEAU_MSG(fmt,args...) ErrorF(fmt, ##args)
#define NOUVEAU_ERR(fmt,args...) \
	ErrorF("%s:%d - "fmt, __func__, __LINE__, ##args)
#if 0
#define NOUVEAU_FALLBACK(fmt,args...) do {    \
	NOUVEAU_ERR("FALLBACK: "fmt, ##args); \
	return FALSE;                         \
} while(0)
#else
#define NOUVEAU_FALLBACK(fmt,args...) do {    \
	return FALSE;                         \
} while(0)
#endif

#define NOUVEAU_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

/* Alternate versions of OUT_RELOCx, takes pixmaps instead of BOs */
#define OUT_PIXMAPd(chan,pm,data,flags,vor,tor) do {                           \
	OUT_RELOCd((chan), pNv->FB, (data), (flags), (vor), (tor));            \
} while(0)
#define OUT_PIXMAPo(chan,pm,flags) do {                                        \
	OUT_RELOCo((chan), pNv->FB, (flags));                                  \
} while(0)
#define OUT_PIXMAPl(chan,pm,delta,flags) do {                                  \
	OUT_RELOCl((chan), pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));\
} while(0)
#define OUT_PIXMAPh(chan,pm,delta,flags) do {                                  \
	OUT_RELOCh((chan), pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));\
} while(0)

#endif
