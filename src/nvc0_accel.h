#ifndef __NVC0_ACCEL_H__
#define __NVC0_ACCEL_H__

#include "hwdefs/nv_object.xml.h"
#include "hwdefs/nv50_2d.xml.h"
#include "hwdefs/nvc0_3d.xml.h"
#include "hwdefs/nvc0_m2mf.xml.h"
#include "hwdefs/nv50_defs.xml.h"
#include "hwdefs/nv50_texture.h"
#include "hwdefs/nv_3ddefs.xml.h"

/* subchannel assignments, compatible with kepler's fixed layout  */
#define SUBC_3D(mthd)    0, (mthd)
#define NVC0_3D(mthd)    SUBC_3D(NVC0_3D_##mthd)
#define SUBC_M2MF(mthd)  2, (mthd)
#define NVC0_M2MF(mthd)  SUBC_M2MF(NVC0_M2MF_##mthd)
#define SUBC_2D(mthd)    3, (mthd)
#define NV50_2D(mthd)    SUBC_2D(NV50_2D_##mthd)
#define NVC0_2D(mthd)    SUBC_2D(NVC0_2D_##mthd)
#define SUBC_NVSW(mthd)  5, (mthd)

/* scratch buffer offsets */
#define CODE_OFFSET 0x00000 /* Code */
#define TIC_OFFSET  0x02000 /* Texture Image Control */
#define TSC_OFFSET  0x03000 /* Texture Sampler Control */
#define NTFY_OFFSET 0x08000
#define MISC_OFFSET 0x10000

/* fragment programs */
#define PFP_S     0x0000 /* (src) */
#define PFP_C     0x0100 /* (src IN mask) */
#define PFP_CCA   0x0200 /* (src IN mask) component-alpha */
#define PFP_CCASA 0x0300 /* (src IN mask) component-alpha src-alpha */
#define PFP_S_A8  0x0400 /* (src) a8 rt */
#define PFP_C_A8  0x0500 /* (src IN mask) a8 rt - same for CA and CA_SA */
#define PFP_NV12  0x0600 /* NV12 YUV->RGB */

/* vertex programs */
#define PVP_PASS  0x0700 /* vertex pass-through shader */

/* shader constants */
#define CB_OFFSET 0x1000

#define VTX_ATTR(a, c, t, s)				\
	((NVC0_3D_VTX_ATTR_DEFINE_TYPE_##t) |		\
	 ((a) << NVC0_3D_VTX_ATTR_DEFINE_ATTR__SHIFT) |	\
	 ((c) << NVC0_3D_VTX_ATTR_DEFINE_COMP__SHIFT) |	\
	 ((s) << NVC0_3D_VTX_ATTR_DEFINE_SIZE__SHIFT))

static __inline__ void
VTX1s(NVPtr pNv, float sx, float sy, unsigned dx, unsigned dy)
{
	struct nouveau_pushbuf *push = pNv->pushbuf;

	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 3);
	PUSH_DATA (push, VTX_ATTR(1, 2, FLOAT, 4));
	PUSH_DATAf(push, sx);
	PUSH_DATAf(push, sy);
#if 1
	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 2);
	PUSH_DATA (push, VTX_ATTR(0, 2, USCALED, 2));
	PUSH_DATA (push, (dy << 16) | dx);
#else
	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 3);
	PUSH_DATA (push, VTX_ATTR(0, 2, FLOAT, 4));
	PUSH_DATAf(push, (float)dx);
	PUSH_DATAf(push, (float)dy);
#endif
}

static __inline__ void
VTX2s(NVPtr pNv, float s1x, float s1y, float s2x, float s2y,
      unsigned dx, unsigned dy)
{
	struct nouveau_pushbuf *push = pNv->pushbuf;

	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 3);
	PUSH_DATA (push, VTX_ATTR(1, 2, FLOAT, 4));
	PUSH_DATAf(push, s1x);
	PUSH_DATAf(push, s1y);
	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 3);
	PUSH_DATA (push, VTX_ATTR(2, 2, FLOAT, 4));
	PUSH_DATAf(push, s2x);
	PUSH_DATAf(push, s2y);
#if 1
	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 2);
	PUSH_DATA (push, VTX_ATTR(0, 2, USCALED, 2));
	PUSH_DATA (push, (dy << 16) | dx);
#else
	BEGIN_NVC0(push, NVC0_3D(VTX_ATTR_DEFINE), 3);
	PUSH_DATA (push, VTX_ATTR(0, 2, FLOAT, 4));
	PUSH_DATAf(push, (float)dx);
	PUSH_DATAf(push, (float)dy);
#endif
}

static __inline__ void
PUSH_DATAu(struct nouveau_pushbuf *push, struct nouveau_bo *bo,
	   unsigned delta, unsigned dwords)
{
	BEGIN_NVC0(push, NVC0_M2MF(OFFSET_OUT_HIGH), 2);
	PUSH_DATA (push, (bo->offset + delta) >> 32);
	PUSH_DATA (push, (bo->offset + delta));
	BEGIN_NVC0(push, NVC0_M2MF(LINE_LENGTH_IN), 2);
	PUSH_DATA (push, dwords * 4);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NVC0_M2MF(EXEC), 1);
	PUSH_DATA (push, 0x100111);
	BEGIN_NIC0(push, NVC0_M2MF(DATA), dwords);
}

#endif
