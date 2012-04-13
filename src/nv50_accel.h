#ifndef __NV50_ACCEL_H__
#define __NV50_ACCEL_H__


#include "hwdefs/nv50_2d.xml.h"
#include "hwdefs/nv50_3d.xml.h"
#include "hwdefs/nv50_defs.xml.h"
#include "hwdefs/nv50_texture.h"
#include "hwdefs/nv_3ddefs.xml.h"
#include "hwdefs/nv_m2mf.xml.h"

/* subchannel assignments */
#define SUBC_M2MF(mthd)  0, (mthd)
#define NV03_M2MF(mthd)  SUBC_M2MF(NV03_M2MF_##mthd)
#define NV50_M2MF(mthd)  SUBC_M2MF(NV50_M2MF_##mthd)
#define SUBC_NVSW(mthd)  1, (mthd)
#define SUBC_2D(mthd)    2, (mthd)
#define NV50_2D(mthd)    SUBC_2D(NV50_2D_##mthd)
#define NVC0_2D(mthd)    SUBC_2D(NVC0_2D_##mthd)
#define SUBC_3D(mthd)    7, (mthd)
#define NV50_3D(mthd)    SUBC_3D(NV50_3D_##mthd)

/* "Tesla scratch buffer" offsets */
#define PVP_OFFSET  0x00000000 /* Vertex program */
#define PFP_OFFSET  0x00001000 /* Fragment program */
#define TIC_OFFSET  0x00002000 /* Texture Image Control */
#define TSC_OFFSET  0x00003000 /* Texture Sampler Control */
#define PFP_DATA    0x00004000 /* FP constbuf */

/* Fragment programs */
#define PFP_S     0x0000 /* (src) */
#define PFP_C     0x0100 /* (src IN mask) */
#define PFP_CCA   0x0200 /* (src IN mask) component-alpha */
#define PFP_CCASA 0x0300 /* (src IN mask) component-alpha src-alpha */
#define PFP_S_A8  0x0400 /* (src) a8 rt */
#define PFP_C_A8  0x0500 /* (src IN mask) a8 rt - same for CA and CA_SA */
#define PFP_NV12  0x0600 /* NV12 YUV->RGB */

/* Constant buffer assignments */
#define CB_TSC 0
#define CB_TIC 1
#define CB_PFP 2

static __inline__ void
VTX1s(NVPtr pNv, float sx, float sy, unsigned dx, unsigned dy)
{
	struct nouveau_pushbuf *push = pNv->pushbuf;

	BEGIN_NV04(push, NV50_3D(VTX_ATTR_2F_X(8)), 2);
	PUSH_DATAf(push, sx);
	PUSH_DATAf(push, sy);
	BEGIN_NV04(push, NV50_3D(VTX_ATTR_2I(0)), 1);
 	PUSH_DATA (push, (dy << 16) | dx);
}

static __inline__ void
VTX2s(NVPtr pNv, float s1x, float s1y, float s2x, float s2y,
		 unsigned dx, unsigned dy)
{
	struct nouveau_pushbuf *push = pNv->pushbuf;

	BEGIN_NV04(push, NV50_3D(VTX_ATTR_2F_X(8)), 4);
	PUSH_DATAf(push, s1x);
	PUSH_DATAf(push, s1y);
	PUSH_DATAf(push, s2x);
	PUSH_DATAf(push, s2y);
	BEGIN_NV04(push, NV50_3D(VTX_ATTR_2I(0)), 1);
 	PUSH_DATA (push, (dy << 16) | dx);
}

#endif
