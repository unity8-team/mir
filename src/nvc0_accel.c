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

#include "nv_include.h"
#include "nvc0_accel.h"

Bool
NVAccelInitM2MF_NVC0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	int ret;

	ret = nouveau_object_new(pNv->channel, 0x00009039, 0x9039,
				 NULL, 0, &pNv->NvMemFormat);
	if (ret)
		return FALSE;

	BEGIN_NVC0(push, NV01_SUBC(M2MF, OBJECT), 1);
	PUSH_DATA (push, pNv->NvMemFormat->handle);
	return TRUE;
}

Bool
NVAccelInit2D_NVC0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	int ret;

	ret = nouveau_object_new(pNv->channel, 0x0000902d, 0x902d,
				 NULL, 0, &pNv->Nv2D);
	if (ret)
		return FALSE;

	if (!PUSH_SPACE(push, 64))
		return FALSE;

	BEGIN_NVC0(push, NV01_SUBC(2D, OBJECT), 1);
	PUSH_DATA (push, pNv->Nv2D->handle);

	BEGIN_NVC0(push, NV50_2D(CLIP_ENABLE), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NV50_2D(COLOR_KEY_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_2D(0x0884), 1);
	PUSH_DATA (push, 0x3f);
	BEGIN_NVC0(push, SUBC_2D(0x0888), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NV50_2D(ROP), 1);
	PUSH_DATA (push, 0x55);
	BEGIN_NVC0(push, NV50_2D(OPERATION), 1);
	PUSH_DATA (push, NV50_2D_OPERATION_SRCCOPY);

	BEGIN_NVC0(push, NV50_2D(BLIT_DU_DX_FRACT), 4);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 1);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NV50_2D(DRAW_SHAPE), 2);
	PUSH_DATA (push, 4);
	PUSH_DATA (push, NV50_SURFACE_FORMAT_B5G6R5_UNORM);
	BEGIN_NVC0(push, NV50_2D(PATTERN_COLOR_FORMAT), 2);
	PUSH_DATA (push, 2);
	PUSH_DATA (push, 1);

	pNv->currentRop = 0xfffffffa;
	return TRUE;
}

Bool
NVAccelInit3D_NVC0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	struct nouveau_bo *bo;
	int ret, i;

	ret = nouveau_object_new(pNv->channel, 0x00009097, 0x9097,
				 NULL, 0, &pNv->Nv3D);
	if (ret)
		return FALSE;

	ret = nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM,
			     (128 << 10), 0x20000, NULL,
			     &pNv->tesla_scratch);
	if (ret) {
		nouveau_object_del(&pNv->Nv3D);
		return FALSE;
	}
	bo = pNv->tesla_scratch;

	if (nouveau_pushbuf_space(push, 512, 0, 0) ||
	    nouveau_pushbuf_refn (push, &(struct nouveau_pushbuf_refn) {
					pNv->tesla_scratch, NOUVEAU_BO_VRAM |
					NOUVEAU_BO_WR }, 1))
		return FALSE;

	BEGIN_NVC0(push, NVC0_M2MF(QUERY_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + NTFY_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + NTFY_OFFSET));
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NV01_SUBC(3D, OBJECT), 1);
	PUSH_DATA (push, pNv->Nv3D->handle);
	BEGIN_NVC0(push, SUBC_3D(NVC0_GRAPH_NOTIFY_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + NTFY_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + NTFY_OFFSET));
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(CSAA_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(MULTISAMPLE_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(MULTISAMPLE_MODE), 1);
	PUSH_DATA (push, NVC0_3D_MULTISAMPLE_MODE_MS1);

	BEGIN_NVC0(push, NVC0_3D(COND_MODE), 1);
	PUSH_DATA (push, NVC0_3D_COND_MODE_ALWAYS);
	BEGIN_NVC0(push, NVC0_3D(RT_CONTROL), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NVC0_3D(ZETA_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(CLIP_RECTS_EN), 2);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(CLIPID_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(VERTEX_TWO_SIDE_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_3D(0x0fac), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(COLOR_MASK(0)), 8);
	PUSH_DATA (push, 0x1111);
	for (i = 1; i < 8; ++i)
		PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(SCREEN_SCISSOR_HORIZ), 2);
	PUSH_DATA (push, (8192 << 16) | 0);
	PUSH_DATA (push, (8192 << 16) | 0);
	BEGIN_NVC0(push, NVC0_3D(SCREEN_Y_CONTROL), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(WINDOW_OFFSET_X), 2);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_3D(0x1590), 1);
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(LINKED_TSC), 1);
	PUSH_DATA (push, 1);

	BEGIN_NVC0(push, NVC0_3D(VIEWPORT_TRANSFORM_EN), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(VIEW_VOLUME_CLIP_CTRL), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(DEPTH_RANGE_NEAR(0)), 2);
	PUSH_DATAf(push, 0.0f);
	PUSH_DATAf(push, 1.0f);

	BEGIN_NVC0(push, NVC0_3D(TIC_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + TIC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + TIC_OFFSET));
	PUSH_DATA (push, 15);
	BEGIN_NVC0(push, NVC0_3D(TSC_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + TSC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + TSC_OFFSET));
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(TEX_LIMITS(4)), 1);
	PUSH_DATA (push, 0x54);

	BEGIN_NVC0(push, NVC0_3D(BLEND_ENABLE(0)), 8);
	PUSH_DATA (push, 1);
	for (i = 1; i < 8; ++i)
		PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(BLEND_INDEPENDENT), 1);
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, SUBC_3D(0x17bc), 3);
	PUSH_DATA (push, (bo->offset + MISC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + MISC_OFFSET));
	PUSH_DATA (push, 1);

	BEGIN_NVC0(push, NVC0_3D(CODE_ADDRESS_HIGH), 2);
	PUSH_DATA (push, (bo->offset + CODE_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + CODE_OFFSET));

	PUSH_DATAu(push, bo, PVP_PASS, 20 + 7 * 2);
	PUSH_DATA (push, 0x00020461);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, 0xff000);
	PUSH_DATA (push, 0x00000000); /* VP_ATTR_EN[0x000] */
	PUSH_DATA (push, 0x0001033f); /* VP_ATTR_EN[0x080] */
	PUSH_DATA (push, 0x00000000); /* VP_ATTR_EN[0x100] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000); /* VP_ATTR_EN[0x200] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000); /* VP_ATTR_EN[0x300] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0033f000); /* VP_EXPORT_EN[0x040] */
	PUSH_DATA (push, 0x00000000); /* VP_EXPORT_EN[0x0c0] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000); /* VP_EXPORT_EN[0x2c0] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c66);
	PUSH_DATA (push, 0x06000080); /* vfetch { $r0,1,2,3 } b128 a[0x80] */
	PUSH_DATA (push, 0xfff11c26);
	PUSH_DATA (push, 0x06000090); /* vfetch { $r4,5 } b64 a[0x90] */
	PUSH_DATA (push, 0xfff19c26);
	PUSH_DATA (push, 0x060000a0); /* vfetch { $r6,7 } b64 a[0xa0] */
	PUSH_DATA (push, 0x03f01c66);
	PUSH_DATA (push, 0x0a7e0070); /* export v[0x70] { $r0 $r1 $r2 $r3 } */
	PUSH_DATA (push, 0x13f01c26);
	PUSH_DATA (push, 0x0a7e0080); /* export v[0x80] { $r4 $r5 } */
	PUSH_DATA (push, 0x1bf01c26);
	PUSH_DATA (push, 0x0a7e0090); /* export v[0x90] { $r6 $r7 } */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(1)), 2);
	PUSH_DATA (push, 0x11);
	PUSH_DATA (push, PVP_PASS);
	BEGIN_NVC0(push, NVC0_3D(SP_GPR_ALLOC(1)), 1);
	PUSH_DATA (push, 8);
	BEGIN_NVC0(push, SUBC_3D(0x163c), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_3D(0x2600), 1);
	PUSH_DATA (push, 1);

	PUSH_DATAu(push, bo, PFP_S, 20 + 6 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x0000000a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f05c40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f01c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x8013c000); /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_C, 20 + 13 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x00000a0a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f0dc40);
	PUSH_DATA (push, 0xc07e0094); /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	PUSH_DATA (push, 0x03f09c40);
	PUSH_DATA (push, 0xc07e0090); /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	PUSH_DATA (push, 0xfc211e86);
	PUSH_DATA (push, 0x80120001); /* tex { _,_,_,$r4 } $t1 { $r2,3 } */
	PUSH_DATA (push, 0x03f05c40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f01c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x8013c000); /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	PUSH_DATA (push, 0x1030dc40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r3 $r3 $r4 */
	PUSH_DATA (push, 0x10209c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r2 $r2 $r4 */
	PUSH_DATA (push, 0x10105c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r1 $r1 $r4 */
	PUSH_DATA (push, 0x10001c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r0 $r0 $r4 */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_CCA, 20 + 13 * 2);
	PUSH_DATA (push, 0x00021462); /* 0x0000c000 = USES_KIL, MULTI_COLORS */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000); /* FRAG_COORD_UMASK = 0x8 */
	PUSH_DATA (push, 0x00000a0a); /* FP_INTERP[0x080], 0022 0022 */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x0c0], 0 = OFF */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x100], 1 = FLAT */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x140], 2 = PERSPECTIVE */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x180], 3 = LINEAR */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x1c0] */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x200] */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x240] */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x280] */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x2c0] */
	PUSH_DATA (push, 0x00000000); /* FP_INTERP[0x300] */
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f); /* FP_RESULT_MASK (0x8000 Face ?) */
	PUSH_DATA (push, 0x00000000); /* 0x2 = FragDepth, 0x1 = SampleMask */
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f0dc40);
	PUSH_DATA (push, 0xc07e0094); /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	PUSH_DATA (push, 0x03f09c40);
	PUSH_DATA (push, 0xc07e0090); /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	PUSH_DATA (push, 0xfc211e86);
	PUSH_DATA (push, 0x8013c001); /* tex { $r4,5,6,7 } $t1 { $r2,3 } */
	PUSH_DATA (push, 0x03f05c40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f01c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x8013c000); /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	PUSH_DATA (push, 0x1c30dc40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r3 $r3 $r7 */
	PUSH_DATA (push, 0x18209c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r2 $r2 $r6 */
	PUSH_DATA (push, 0x14105c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r1 $r1 $r5 */
	PUSH_DATA (push, 0x10001c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r0 $r0 $r4 */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_CCASA, 20 + 13 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x00000a0a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f0dc40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r3 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f09c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r2 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc211e86);
	PUSH_DATA (push, 0x80120000); /* tex { _,_,_,$r4 } $t0 { $r2,3 } */
	PUSH_DATA (push, 0x03f05c40);
	PUSH_DATA (push, 0xc07e0094); /* pinterp f32 $r1 $r0 v[$r63+0x94] */
	PUSH_DATA (push, 0x03f01c40);
	PUSH_DATA (push, 0xc07e0090); /* pinterp f32 $r0 $r0 v[$r63+0x90] */
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x8013c001); /* tex { $r0,1,2,3 } $t1 { $r0,1 } */
	PUSH_DATA (push, 0x1030dc40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r3 $r3 $r4 */
	PUSH_DATA (push, 0x10209c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r2 $r2 $r4 */
	PUSH_DATA (push, 0x10105c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r1 $r1 $r4 */
	PUSH_DATA (push, 0x10001c40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r0 $r0 $r4 */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_S_A8, 20 + 9 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x0000000a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f05c40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f01c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x80120000); /* tex { _ _ _ $r0 } $t0 { $r0 $r1 } */
	PUSH_DATA (push, 0x0000dde4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r3 $r0 */
	PUSH_DATA (push, 0x00009de4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r2 $r0 */
	PUSH_DATA (push, 0x00005de4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r1 $r0 */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_C_A8, 20 + 13 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x00000a0a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff01c00);
	PUSH_DATA (push, 0xc07e007c); /* linterp f32 $r0 v[$r63+0x7c] */
	PUSH_DATA (push, 0x10001c00);
	PUSH_DATA (push, 0xc8000000); /* rcp f32 $r0 $r0 */
	PUSH_DATA (push, 0x03f0dc40);
	PUSH_DATA (push, 0xc07e0094); /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	PUSH_DATA (push, 0x03f09c40);
	PUSH_DATA (push, 0xc07e0090); /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	PUSH_DATA (push, 0xfc205e86);
	PUSH_DATA (push, 0x80120001); /* tex { _ _ _ $r1 } $t1 { $r2 $r3 } */
	PUSH_DATA (push, 0x03f0dc40);
	PUSH_DATA (push, 0xc07e0084); /* pinterp f32 $r3 $r0 v[$r63+0x84] */
	PUSH_DATA (push, 0x03f09c40);
	PUSH_DATA (push, 0xc07e0080); /* pinterp f32 $r2 $r0 v[$r63+0x80] */
	PUSH_DATA (push, 0xfc201e86);
	PUSH_DATA (push, 0x80120000); /* tex { _ _ _ $r0 } $t0 { $r2 $r3 } */
	PUSH_DATA (push, 0x0400dc40);
	PUSH_DATA (push, 0x58000000); /* mul ftz rn f32 $r3 $r0 $r1 */
	PUSH_DATA (push, 0x0c009de4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r2 $r3 */
	PUSH_DATA (push, 0x0c005de4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r1 $r3 */
	PUSH_DATA (push, 0x0c001de4);
	PUSH_DATA (push, 0x28000000); /* mov b32 $r0 $r3 */
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000); /* exit */

	PUSH_DATAu(push, bo, PFP_NV12, 20 + 19 * 2);
	PUSH_DATA (push, 0x00021462);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x80000000);
	PUSH_DATA (push, 0x00000a0a);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0x0000000f);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 0xfff09c00);
	PUSH_DATA (push, 0xc07e007c);
	PUSH_DATA (push, 0x10209c00);
	PUSH_DATA (push, 0xc8000000);
	PUSH_DATA (push, 0x0bf01c40);
	PUSH_DATA (push, 0xc07e0080);
	PUSH_DATA (push, 0x0bf05c40);
	PUSH_DATA (push, 0xc07e0084);
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x80120000);
	PUSH_DATA (push, 0x00015c40);
	PUSH_DATA (push, 0x58004000);
	PUSH_DATA (push, 0x1050dc20);
	PUSH_DATA (push, 0x50004000);
	PUSH_DATA (push, 0x20511c20);
	PUSH_DATA (push, 0x50004000);
	PUSH_DATA (push, 0x30515c20);
	PUSH_DATA (push, 0x50004000);
	PUSH_DATA (push, 0x0bf01c40);
	PUSH_DATA (push, 0xc07e0090);
	PUSH_DATA (push, 0x0bf05c40);
	PUSH_DATA (push, 0xc07e0094);
	PUSH_DATA (push, 0xfc001e86);
	PUSH_DATA (push, 0x80130001);
	PUSH_DATA (push, 0x4000dc40);
	PUSH_DATA (push, 0x30064000);
	PUSH_DATA (push, 0x50011c40);
	PUSH_DATA (push, 0x30084000);
	PUSH_DATA (push, 0x60015c40);
	PUSH_DATA (push, 0x300a4000);
	PUSH_DATA (push, 0x70101c40);
	PUSH_DATA (push, 0x30064000);
	PUSH_DATA (push, 0x90109c40);
	PUSH_DATA (push, 0x300a4000);
	PUSH_DATA (push, 0x80105c40);
	PUSH_DATA (push, 0x30084000);
	PUSH_DATA (push, 0x00001de7);
	PUSH_DATA (push, 0x80000000);

	BEGIN_NVC0(push, SUBC_3D(0x021c), 1); /* CODE_FLUSH ? */
	PUSH_DATA (push, 0x1111);

	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(5)), 2);
	PUSH_DATA (push, 0x51);
	PUSH_DATA (push, PFP_S);
	BEGIN_NVC0(push, NVC0_3D(SP_GPR_ALLOC(5)), 1);
	PUSH_DATA (push, 8);

	BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
	PUSH_DATA (push, 256);
	PUSH_DATA (push, (bo->offset + CB_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + CB_OFFSET));
	BEGIN_NVC0(push, NVC0_3D(CB_BIND(4)), 1);
	PUSH_DATA (push, 0x01);

	BEGIN_NVC0(push, NVC0_3D(EARLY_FRAGMENT_TESTS), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_3D(0x0360), 2);
	PUSH_DATA (push, 0x20164010);
	PUSH_DATA (push, 0x20);
	BEGIN_NVC0(push, SUBC_3D(0x196c), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, SUBC_3D(0x1664), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(FRAG_COLOR_CLAMP_EN), 1);
	PUSH_DATA (push, 0x11111111);

	BEGIN_NVC0(push, NVC0_3D(DEPTH_TEST_ENABLE), 1);
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(RASTERIZE_ENABLE), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(4)), 1);
	PUSH_DATA (push, 0x40);
	BEGIN_NVC0(push, NVC0_3D(LAYER), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(3)), 1);
	PUSH_DATA (push, 0x30);
	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(2)), 1);
	PUSH_DATA (push, 0x20);
	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(0)), 1);
	PUSH_DATA (push, 0x00);

	BEGIN_NVC0(push, SUBC_3D(0x1604), 1);
	PUSH_DATA (push, 4);
	BEGIN_NVC0(push, NVC0_3D(POINT_SPRITE_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(SCISSOR_ENABLE(0)), 1);
	PUSH_DATA (push, 1);

	BEGIN_NVC0(push, NVC0_3D(VIEWPORT_HORIZ(0)), 2);
	PUSH_DATA (push, (8192 << 16) | 0);
	PUSH_DATA (push, (8192 << 16) | 0);
	BEGIN_NVC0(push, NVC0_3D(SCISSOR_HORIZ(0)), 2);
	PUSH_DATA (push, (8192 << 16) | 0);
	PUSH_DATA (push, (8192 << 16) | 0);
	return TRUE;
}

