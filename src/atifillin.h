/*
 * atifillin.h: header for atifillin.c.
 *
 * (c) 2004 Adam Jackson.  Standard MIT license applies.
 */

#ifndef ATI_FILLIN_H
#define ATI_FILLIN_H

/* include headers corresponding to fields touched by ATIFillInScreenInfo() */

#include "ativersion.h"
#include "atiprobe.h"
#include "atipreinit.h"
#include "atiscreen.h"
#include "aticonsole.h"
#include "atiadjust.h"
#include "ativalid.h"

extern void ATIFillInScreenInfo FunctionPrototype((ScrnInfoPtr));

#endif
