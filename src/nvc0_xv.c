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
#include "nvc0_accel.h"

extern Atom xvSyncToVBlank, xvSetDefaults;

void
nvc0_xv_m2mf(struct nouveau_grobj *m2mf,
	     struct nouveau_bo *dst, int uv_offset, int dst_pitch, int nlines,
	     struct nouveau_bo *src, int line_len)
{
	struct nouveau_channel *chan = m2mf->channel;

	BEGIN_RING(chan, m2mf, NVC0_M2MF_TILING_MODE_OUT, 5);
	OUT_RING  (chan, dst->tile_mode);
	OUT_RING  (chan, dst_pitch);
	OUT_RING  (chan, nlines);
	OUT_RING  (chan, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_TILING_POSITION_OUT_X, 2);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 0);

	if (uv_offset) {
		BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_IN_HIGH, 2);
		OUT_RELOCh(chan, src, line_len * nlines,
				 NOUVEAU_BO_GART | NOUVEAU_BO_RD);
		OUT_RELOCl(chan, src, line_len * nlines,
				 NOUVEAU_BO_GART | NOUVEAU_BO_RD);
		BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_OUT_HIGH, 2);
		OUT_RELOCh(chan, dst, uv_offset,
				 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
		OUT_RELOCl(chan, dst, uv_offset,
				 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
		BEGIN_RING(chan, m2mf, NVC0_M2MF_PITCH_IN, 4);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, dst_pitch);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, nlines >> 1);
		BEGIN_RING(chan, m2mf, NVC0_M2MF_EXEC, 1);
		OUT_RING  (chan, 0x00100010);
	}

	BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_IN_HIGH, 2);
	OUT_RELOCh(chan, src, 0, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
	OUT_RELOCl(chan, src, 0, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_OUT_HIGH, 2);
	OUT_RELOCh(chan, dst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, dst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_PITCH_IN, 4);
	OUT_RING  (chan, line_len);
	OUT_RING  (chan, dst_pitch);
	OUT_RING  (chan, line_len);
	OUT_RING  (chan, nlines);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_EXEC, 1);
	OUT_RING  (chan, 0x00100010);
}

static Bool
nvc0_xv_check_image_put(PixmapPtr ppix)
{
	switch (ppix->drawable.bitsPerPixel) {
	case 32:
	case 24:
	case 16:
	case 15:
		break;
	default:
		return FALSE;
	}

	if (!nv50_style_tiled_pixmap(ppix))
		return FALSE;

	return TRUE;
}

static Bool
nvc0_xv_state_emit(PixmapPtr ppix, int id, struct nouveau_bo *src,
		   int packed_y, int uv, int src_w, int src_h)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_bo *bo = nouveau_pixmap_bo(ppix);
	struct nouveau_grobj *m2mf = pNv->NvMemFormat;
	struct nouveau_grobj *fermi = pNv->Nv3D;
	const unsigned shd_flags = NOUVEAU_BO_RD | NOUVEAU_BO_VRAM;
	const unsigned tcb_flags = NOUVEAU_BO_RDWR | NOUVEAU_BO_VRAM;
	uint32_t mode = 0xd0005000 | (src->tile_mode << 18);

	if (MARK_RING(chan, 256, 18))
		return FALSE;

	BEGIN_RING(chan, fermi, NVC0_3D_RT_ADDRESS_HIGH(0), 8);
	if (OUT_RELOCh(chan, bo, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR) ||
	    OUT_RELOCl(chan, bo, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, ppix->drawable.width);
	OUT_RING  (chan, ppix->drawable.height);
	switch (ppix->drawable.bitsPerPixel) {
	case 32: OUT_RING  (chan, NV50_SURFACE_FORMAT_BGRA8_UNORM); break;
	case 24: OUT_RING  (chan, NV50_SURFACE_FORMAT_BGRX8_UNORM); break;
	case 16: OUT_RING  (chan, NV50_SURFACE_FORMAT_B5G6R5_UNORM); break;
	case 15: OUT_RING  (chan, NV50_SURFACE_FORMAT_BGR5_X1_UNORM); break;
	}
	OUT_RING  (chan, bo->tile_mode);
	OUT_RING  (chan, 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, fermi, NVC0_3D_BLEND_ENABLE(0), 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, fermi, NVC0_3D_TIC_ADDRESS_HIGH, 3);
	if (OUT_RELOCh(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags) ||
	    OUT_RELOCl(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 15);

	BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_OUT_HIGH, 2);
	if (OUT_RELOCh(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags) ||
	    OUT_RELOCl(chan, pNv->tesla_scratch, TIC_OFFSET, tcb_flags)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	BEGIN_RING(chan, m2mf, NVC0_M2MF_LINE_LENGTH_IN, 2);
	OUT_RING  (chan, 16 * 4);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_EXEC, 1);
	OUT_RING  (chan, 0x00100111);
	BEGIN_RING_NI(chan, m2mf, NVC0_M2MF_DATA, 16);
	if (id == FOURCC_YV12 || id == FOURCC_I420) {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C0 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8);
	if (OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
	    OUT_RELOC (chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD |
		       NOUVEAU_BO_HIGH | NOUVEAU_BO_OR, mode, mode)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RING  (chan, 0x00000000);
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C1 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_C0 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8_8);
	if (OUT_RELOCl(chan, src, uv, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
	    OUT_RELOC (chan, src, uv, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD |
		       NOUVEAU_BO_HIGH | NOUVEAU_BO_OR, mode, mode)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w >> 1);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | (src_h >> 1));
	OUT_RING  (chan, 0x03000000);
	OUT_RING  (chan, 0x00000000);
	} else {
	if (id == FOURCC_UYVY) {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C1 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8_8);
	} else {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C0 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8_8);
	}
	if (OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
	    OUT_RELOC (chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD |
		       NOUVEAU_BO_HIGH | NOUVEAU_BO_OR, mode, mode)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, src_w);
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RING  (chan, 0x00000000);
	if (id == FOURCC_UYVY) {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C2 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_C0 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
	} else {
	OUT_RING  (chan, NV50TIC_0_0_MAPA_C3 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPB_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
	}
	if (OUT_RELOCl(chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
	    OUT_RELOC (chan, src, packed_y, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD |
		       NOUVEAU_BO_HIGH | NOUVEAU_BO_OR, mode, mode)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 0x00300000);
	OUT_RING  (chan, (src_w >> 1));
	OUT_RING  (chan, (1 << NV50TIC_0_5_DEPTH_SHIFT) | src_h);
	OUT_RING  (chan, 0x03000000);
	OUT_RING  (chan, 0x00000000);
	}

	BEGIN_RING(chan, fermi, NVC0_3D_TSC_ADDRESS_HIGH, 3);
	if (OUT_RELOCh(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags) ||
	    OUT_RELOCl(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	OUT_RING  (chan, 0x00000000);

	BEGIN_RING(chan, m2mf, NVC0_M2MF_OFFSET_OUT_HIGH, 2);
	if (OUT_RELOCh(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags) ||
	    OUT_RELOCl(chan, pNv->tesla_scratch, TSC_OFFSET, tcb_flags)) {
		MARK_UNDO(chan);
		return FALSE;
	}
	BEGIN_RING(chan, m2mf, NVC0_M2MF_LINE_LENGTH_IN, 2);
	OUT_RING  (chan, 16 * 4);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, m2mf, NVC0_M2MF_EXEC, 1);
	OUT_RING  (chan, 0x00100111);
	BEGIN_RING_NI(chan, m2mf, NVC0_M2MF_DATA, 16);
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

	BEGIN_RING(chan, fermi, NVC0_3D_CODE_ADDRESS_HIGH, 2);
	if (OUT_RELOCh(chan, pNv->tesla_scratch, CODE_OFFSET, shd_flags) ||
	    OUT_RELOCl(chan, pNv->tesla_scratch, CODE_OFFSET, shd_flags)) {
		MARK_UNDO(chan);
		return FALSE;

	}
	BEGIN_RING(chan, fermi, NVC0_3D_SP_START_ID(5), 1);
	OUT_RING  (chan, PFP_NV12);

	BEGIN_RING(chan, fermi, NVC0_3D_TSC_FLUSH, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, fermi, NVC0_3D_TIC_FLUSH, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, fermi, NVC0_3D_TEX_CACHE_CTL, 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, fermi, NVC0_3D_BIND_TIC(4), 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, fermi, NVC0_3D_BIND_TIC(4), 1);
	OUT_RING  (chan, 0x203);

	return TRUE;
}

int
nvc0_xv_image_put(ScrnInfoPtr pScrn,
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
	struct nouveau_grobj *fermi = pNv->Nv3D;
	float X1, X2, Y1, Y2;
	BoxPtr pbox;
	int nbox;

	if (!nvc0_xv_check_image_put(ppix))
		return BadMatch;
	if (!nvc0_xv_state_emit(ppix, id, src, packed_y, uv, width, height))
		return BadAlloc;

	if (0 && pPriv->SyncToVBlank) {
		NV50SyncToVBlank(ppix, dstBox);
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
			if (!nvc0_xv_state_emit(ppix, id, src, packed_y, uv,
						width, height))
				return BadAlloc;
		}

		BEGIN_RING(chan, fermi, NVC0_3D_SCISSOR_HORIZ(0), 2);
		OUT_RING  (chan, sx2 << NVC0_3D_SCISSOR_HORIZ_MAX__SHIFT | sx1);
		OUT_RING  (chan, sy2 << NVC0_3D_SCISSOR_VERT_MAX__SHIFT | sy1 );

		BEGIN_RING(chan, fermi, NVC0_3D_VERTEX_BEGIN_GL, 1);
		OUT_RING  (chan, NVC0_3D_VERTEX_BEGIN_GL_PRIMITIVE_TRIANGLES);
		VTX2s(pNv, tx1, ty1, tx1, ty1, sx1, sy1);
		VTX2s(pNv, tx2+(tx2-tx1), ty1, tx2+(tx2-tx1), ty1, sx2+(sx2-sx1), sy1);
		VTX2s(pNv, tx1, ty2+(ty2-ty1), tx1, ty2+(ty2-ty1), sx1, sy2+(sy2-sy1));
		BEGIN_RING(chan, fermi, NVC0_3D_VERTEX_END_GL, 1);
		OUT_RING  (chan, 0);

		pbox++;
	}

	FIRE_RING (chan);
	return Success;
}

void
nvc0_xv_csc_update(NVPtr pNv, float yco, float *off, float *uco, float *vco)
{
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_bo *bo = pNv->tesla_scratch;
	struct nouveau_grobj *fermi = pNv->Nv3D;

	if (MARK_RING(chan, 64, 2))
		return;

	BEGIN_RING(chan, fermi, NVC0_3D_CB_SIZE, 3);
	OUT_RING  (chan, 256);
	if (OUT_RELOCh(chan, bo, CB_OFFSET, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR) ||
	    OUT_RELOCl(chan, bo, CB_OFFSET, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR)) {
		MARK_UNDO(chan);
		return;
	}
	BEGIN_RING(chan, fermi, NVC0_3D_CB_POS, 11);
	OUT_RING  (chan, 0);
	OUT_RINGf (chan, yco);
	OUT_RINGf (chan, off[0]);
	OUT_RINGf (chan, off[1]);
	OUT_RINGf (chan, off[2]);
	OUT_RINGf (chan, uco[0]);
	OUT_RINGf (chan, uco[1]);
	OUT_RINGf (chan, uco[2]);
	OUT_RINGf (chan, vco[0]);
	OUT_RINGf (chan, vco[1]);
	OUT_RINGf (chan, vco[2]);
}
