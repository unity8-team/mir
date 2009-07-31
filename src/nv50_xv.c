/*
 * Copyright 2008 Ben Skeggs
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "exa.h"
#include "damage.h"
#include "dixstruct.h"
#include "fourcc.h"

#include "nv_include.h"
#include "nv_dma.h"
#include "nv50_accel.h"
#include "nv50_texture.h"

static Bool
nv50_xv_check_image_put(PixmapPtr ppix)
{
	switch (ppix->drawable.depth) {
	case 32:
	case 24:
	case 16:
		break;
	default:
		return FALSE;
	}

	if (!nouveau_exa_pixmap_is_tiled(ppix))
		return FALSE;

	return TRUE;
}

static void
nv50_xv_state_emit(PixmapPtr ppix, int id, struct nouveau_bo *src,
		   int packed_y, int uv, int src_w, int src_h)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *tesla = pNv->Nv3D;
	struct nouveau_bo *bo = nouveau_pixmap_bo(ppix);
	unsigned delta = nouveau_pixmap_offset(ppix);
	const unsigned shd_flags = NOUVEAU_BO_RD | NOUVEAU_BO_VRAM;
	const unsigned tcb_flags = NOUVEAU_BO_RDWR | NOUVEAU_BO_VRAM;

	WAIT_RING (chan, 256);
	BEGIN_RING(chan, tesla, NV50TCL_RT_ADDRESS_HIGH(0), 5);
	OUT_RELOCh(chan, bo, delta, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, bo, delta, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	switch (ppix->drawable.depth) {
	case 32: OUT_RING  (chan, NV50TCL_RT_FORMAT_A8R8G8B8_UNORM); break;
	case 24: OUT_RING  (chan, NV50TCL_RT_FORMAT_X8R8G8B8_UNORM); break;
	case 16: OUT_RING  (chan, NV50TCL_RT_FORMAT_R5G6B5_UNORM); break;
	}
	OUT_RING  (chan, bo->tile_mode << 4);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_RT_HORIZ(0), 2);
	OUT_RING  (chan, ppix->drawable.width);
	OUT_RING  (chan, ppix->drawable.height);
	BEGIN_RING(chan, tesla, 0x1224, 1);
	OUT_RING  (chan, 1);

	BEGIN_RING(chan, tesla, NV50TCL_BLEND_ENABLE(0), 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, tesla, NV50TCL_TIC_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags);
	OUT_RING  (chan, 0x00000800);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags);
	OUT_RING  (chan, (CB_TIC << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, CB_TIC);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 16);
	if (id == FOURCC_YV12 || id == FOURCC_I420) {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C0 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_FMT_8);
	OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, 0xd0005000 | (src->tile_mode << 22));
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RELOCh(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C1 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C0 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_FMT_8_8);
	OUT_RELOCl(chan, src, uv, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, 0xd0005000 | (src->tile_mode << 22));
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w >> 1);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | (src_h >> 1));
	OUT_RING  (chan, 0x03000000);
	OUT_RELOCh(chan, src, uv, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	} else {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C0 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_FMT_8_8);
	OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, 0xd0005000 | (src->tile_mode << 22));
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RELOCh(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C3 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C1 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
	OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (chan, 0xd0005000 | (src->tile_mode << 22));
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, (src_w >> 1));
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RELOCh(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	}

	BEGIN_RING(chan, tesla, NV50TCL_TSC_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags);
	OUT_RING  (chan, 0x00000000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags);
	OUT_RING  (chan, (CB_TSC << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, CB_TSC);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 16);
	OUT_RING  (chan, NV50TSC_1_0_WRAPS_CLAMP_TO_EDGE |
			 NV50TSC_1_0_WRAPT_CLAMP_TO_EDGE |
			 NV50TSC_1_0_WRAPR_CLAMP_TO_EDGE);
	OUT_RING  (chan, NV50TSC_1_1_MAGF_LINEAR |
			 NV50TSC_1_1_MINF_LINEAR |
			 NV50TSC_1_1_MIPF_NONE);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, NV50TSC_1_0_WRAPS_CLAMP_TO_EDGE |
			 NV50TSC_1_0_WRAPT_CLAMP_TO_EDGE |
			 NV50TSC_1_0_WRAPR_CLAMP_TO_EDGE);
	OUT_RING  (chan, NV50TSC_1_1_MAGF_LINEAR |
			 NV50TSC_1_1_MINF_LINEAR |
			 NV50TSC_1_1_MIPF_NONE);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, 0x00000000);

	BEGIN_RING(chan, tesla, NV50TCL_VP_ADDRESS_HIGH, 2);
	OUT_RELOCh(chan, pNv->tesla_scratch, PVP_OFFSET, shd_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, PVP_OFFSET, shd_flags);
	BEGIN_RING(chan, tesla, NV50TCL_FP_ADDRESS_HIGH, 2);
	OUT_RELOCh(chan, pNv->tesla_scratch, PFP_OFFSET, shd_flags);
	OUT_RELOCl(chan, pNv->tesla_scratch, PFP_OFFSET, shd_flags);
	BEGIN_RING(chan, tesla, NV50TCL_FP_START_ID, 1);
	OUT_RING  (chan, PFP_NV12);

	BEGIN_RING(chan, tesla, 0x1334, 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, tesla, 0x1458, 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, tesla, 0x1458, 1);
	OUT_RING  (chan, 0x203);

}

static void
NV50EmitWaitForVBlank(PixmapPtr ppix, int x, int y, int w, int h)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *nvsw = pNv->NvSW;
	int crtcs;

	if (!nouveau_exa_pixmap_is_onscreen(ppix))
		return;

	crtcs = nv_window_belongs_to_crtc(pScrn, x, y, w, h);
	if (!crtcs)
		return;

	BEGIN_RING(chan, nvsw, 0x006c, 1);
	OUT_RING  (chan, 0x22222222);
	BEGIN_RING(chan, nvsw, 0x0404, 2);
	OUT_RING  (chan, 0x11111111);
	OUT_RING  (chan, ffs(crtcs) - 1);
	BEGIN_RING(chan, nvsw, 0x0068, 1);
	OUT_RING  (chan, 0x11111111);
}

int
nv50_xv_image_put(ScrnInfoPtr pScrn,
		  struct nouveau_bo *src, int packed_y, int uv,
		  int id, int src_pitch, BoxPtr dstBox,
		  int x1, int y1, int x2, int y2,
		  uint16_t width, uint16_t height,
		  uint16_t src_w, uint16_t src_h,
		  uint16_t drw_w, uint16_t drw_h,
		  RegionPtr clipBoxes, PixmapPtr ppix,
		  NVPortPrivPtr pPriv)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *tesla = pNv->Nv3D;
	float X1, X2, Y1, Y2;
	BoxPtr pbox;
	int nbox;

	if (!nv50_xv_check_image_put(ppix))
		return BadMatch;
	nv50_xv_state_emit(ppix, id, src, packed_y, uv, width, height);

	if (pPriv->SyncToVBlank) {
		NV50EmitWaitForVBlank(ppix, dstBox->x1, dstBox->y1,
				      dstBox->x2 - dstBox->x1,
				      dstBox->y2 - dstBox->y1);
	}

	/* These are fixed point values in the 16.16 format. */
	X1 = (float)(x1>>16)+(float)(x1&0xFFFF)/(float)0x10000;
	Y1 = (float)(y1>>16)+(float)(y1&0xFFFF)/(float)0x10000;
	X2 = (float)(x2>>16)+(float)(x2&0xFFFF)/(float)0x10000;
	Y2 = (float)(y2>>16)+(float)(y2&0xFFFF)/(float)0x10000;

	pbox = REGION_RECTS(clipBoxes);
	nbox = REGION_NUM_RECTS(clipBoxes);
	while(nbox--) {
		float tx1=X1+(float)(pbox->x1 - dstBox->x1)*(X2-X1)/(float)(drw_w);
		float tx2=X1+(float)(pbox->x2 - dstBox->x1)*(src_w)/(float)(drw_w);
		float ty1=Y1+(float)(pbox->y1 - dstBox->y1)*(Y2-Y1)/(float)(drw_h);
		float ty2=Y1+(float)(pbox->y2 - dstBox->y1)*(src_h)/(float)(drw_h);
		int sx1=pbox->x1;
		int sx2=pbox->x2;
		int sy1=pbox->y1;
		int sy2=pbox->y2;

		tx1 = tx1 / width;
		tx2 = tx2 / width;
		ty1 = ty1 / height;
		ty2 = ty2 / height;

		if (AVAIL_RING(chan) < 64) {
			nv50_xv_state_emit(ppix, id, src, packed_y, uv,
					   src_w, src_h);
		}

		/* NV50TCL_SCISSOR_VERT_T_SHIFT is wrong, because it was deducted with
		* origin lying at the bottom left. This will be changed to _MIN_ and _MAX_
		* later, because it is origin dependent.
		*/
		BEGIN_RING(chan, tesla, NV50TCL_SCISSOR_HORIZ, 2);
		OUT_RING  (chan, sx2 << NV50TCL_SCISSOR_HORIZ_R_SHIFT | sx1);
		OUT_RING  (chan, sy2 << NV50TCL_SCISSOR_VERT_T_SHIFT | sy1 );

		BEGIN_RING(chan, tesla, NV50TCL_VERTEX_BEGIN, 1);
		OUT_RING  (chan, NV50TCL_VERTEX_BEGIN_TRIANGLES);
		VTX2s(pNv, tx1, ty1, tx1, ty1, sx1, sy1);
		VTX2s(pNv, tx2+(tx2-tx1), ty1, tx2+(tx2-tx1), ty1, sx2+(sx2-sx1), sy1);
		VTX2s(pNv, tx1, ty2+(ty2-ty1), tx1, ty2+(ty2-ty1), sx1, sy2+(sy2-sy1));
		BEGIN_RING(chan, tesla, NV50TCL_VERTEX_END, 1);
		OUT_RING  (chan, 0);

		pbox++;
	}

	FIRE_RING (chan);
	return Success;
}

void
nv50_xv_video_stop(ScrnInfoPtr pScrn, pointer data, Bool exit)
{
}

int
nv50_xv_port_attribute_set(ScrnInfoPtr pScrn, Atom attribute,
			   INT32 value, pointer data)
{
	return BadMatch;
}

int
nv50_xv_port_attribute_get(ScrnInfoPtr pScrn, Atom attribute,
			   INT32 *value, pointer data)
{
	return BadMatch;
}

