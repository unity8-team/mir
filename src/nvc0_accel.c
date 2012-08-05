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
#include "nvc0_shader.h"
#include "nve0_shader.h"

void
NVC0SyncToVBlank(PixmapPtr ppix, BoxPtr box)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(ppix->drawable.pScreen);
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	int crtcs;

	if (!pNv->NvSW || !nouveau_exa_pixmap_is_onscreen(ppix))
		return;

	crtcs = nv_window_belongs_to_crtc(pScrn, box->x1, box->y1,
					  box->x2 - box->x1,
					  box->y2 - box->y1);
	if (!crtcs)
		return;

	if (!PUSH_SPACE(push, 32))
		return;

	BEGIN_NVC0(push, NV01_SUBC(NVSW, OBJECT), 1);
	PUSH_DATA (push, pNv->NvSW->handle);
	BEGIN_NVC0(push, NV84_SUBC(NVSW, SEMAPHORE_ADDRESS_HIGH), 4);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET) >> 32);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET));
	PUSH_DATA (push, 0x22222222);
	PUSH_DATA (push, NV84_SUBCHAN_SEMAPHORE_TRIGGER_WRITE_LONG);
	BEGIN_NVC0(push, SUBC_NVSW(0x0400), 4);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET) >> 32);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET));
	PUSH_DATA (push, 0x11111111);
	PUSH_DATA (push, ffs(crtcs) - 1);
	BEGIN_NVC0(push, NV84_SUBC(NVSW, SEMAPHORE_ADDRESS_HIGH), 4);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET) >> 32);
	PUSH_DATA (push, (pNv->scratch->offset + SEMA_OFFSET));
	PUSH_DATA (push, 0x11111111);
	PUSH_DATA (push, NV84_SUBCHAN_SEMAPHORE_TRIGGER_ACQUIRE_EQUAL);
}

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
	BEGIN_NVC0(push, NVC0_M2MF(QUERY_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (pNv->scratch->offset + NTFY_OFFSET) >> 32);
	PUSH_DATA (push, (pNv->scratch->offset + NTFY_OFFSET));
	PUSH_DATA (push, 0);

	return TRUE;
}

Bool
NVAccelInitCopy_NVC0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->ce_pushbuf;
	int ret;

	ret = nouveau_object_new(pNv->ce_channel, 0x000490b5, 0x90b5,
				 NULL, 0, &pNv->NvCopy);
	if (ret)
		return FALSE;

	if (!PUSH_SPACE(push, 2))
		return FALSE;

	BEGIN_NVC0(push, NV01_SUBC(COPY, OBJECT), 1);
	PUSH_DATA (push, pNv->NvCopy->handle);

	return TRUE;
}

Bool
NVAccelInitP2MF_NVE0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	int ret;

	ret = nouveau_object_new(pNv->channel, 0x0000a040, 0xa040,
				 NULL, 0, &pNv->NvMemFormat);
	if (ret)
		return FALSE;

	BEGIN_NVC0(push, NV01_SUBC(P2MF, OBJECT), 1);
	PUSH_DATA (push, pNv->NvMemFormat->handle);
	BEGIN_NVC0(push, NV01_SUBC(COPY, OBJECT), 1);
	PUSH_DATA (push, 0x0000a0b5);
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
	struct nouveau_bo *bo = pNv->scratch;
	uint32_t class, handle;
	int ret;

	if (pNv->Architecture < NV_ARCH_E0) {
		class  = 0x9097;
		handle = 0x001f906e;
	} else {
		class  = 0xa097;
		handle = 0x0000906e;
	}

	ret = nouveau_object_new(pNv->channel, class, class,
				 NULL, 0, &pNv->Nv3D);
	if (ret)
		return FALSE;

	ret = nouveau_object_new(pNv->channel, handle, 0x906e,
				 NULL, 0, &pNv->NvSW);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "DRM doesn't support sync-to-vblank\n");
	}

	if (nouveau_pushbuf_space(push, 512, 0, 0) ||
	    nouveau_pushbuf_refn (push, &(struct nouveau_pushbuf_refn) {
					pNv->scratch, NOUVEAU_BO_VRAM |
					NOUVEAU_BO_WR }, 1))
		return FALSE;

	BEGIN_NVC0(push, NV01_SUBC(3D, OBJECT), 1);
	PUSH_DATA (push, pNv->Nv3D->handle);
	BEGIN_NVC0(push, NVC0_3D(COND_MODE), 1);
	PUSH_DATA (push, NVC0_3D_COND_MODE_ALWAYS);
	BEGIN_NVC0(push, SUBC_3D(NVC0_GRAPH_NOTIFY_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (pNv->scratch->offset + NTFY_OFFSET) >> 32);
	PUSH_DATA (push, (pNv->scratch->offset + NTFY_OFFSET));
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(CSAA_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(ZETA_ENABLE), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(RT_SEPARATE_FRAG_DATA), 1);
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(VIEWPORT_HORIZ(0)), 2);
	PUSH_DATA (push, (8192 << 16) | 0);
	PUSH_DATA (push, (8192 << 16) | 0);
	BEGIN_NVC0(push, NVC0_3D(SCREEN_SCISSOR_HORIZ), 2);
	PUSH_DATA (push, (8192 << 16) | 0);
	PUSH_DATA (push, (8192 << 16) | 0);
	BEGIN_NVC0(push, NVC0_3D(SCISSOR_ENABLE(0)), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NVC0_3D(VIEWPORT_TRANSFORM_EN), 1);
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(VIEW_VOLUME_CLIP_CTRL), 1);
	PUSH_DATA (push, 0);

	BEGIN_NVC0(push, NVC0_3D(TIC_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + TIC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + TIC_OFFSET));
	PUSH_DATA (push, 15);
	BEGIN_NVC0(push, NVC0_3D(TSC_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + TSC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + TSC_OFFSET));
	PUSH_DATA (push, 0);
	BEGIN_NVC0(push, NVC0_3D(LINKED_TSC), 1);
	PUSH_DATA (push, 1);
	if (pNv->Architecture < NV_ARCH_E0) {
		BEGIN_NVC0(push, NVC0_3D(TEX_LIMITS(4)), 1);
		PUSH_DATA (push, 0x54);
		BEGIN_NIC0(push, NVC0_3D(BIND_TIC(4)), 2);
		PUSH_DATA (push, (0 << 9) | (0 << 1) | NVC0_3D_BIND_TIC_ACTIVE);
		PUSH_DATA (push, (1 << 9) | (1 << 1) | NVC0_3D_BIND_TIC_ACTIVE);
	} else {
		BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 6);
		PUSH_DATA (push, 256);
		PUSH_DATA (push, (bo->offset + TB_OFFSET) >> 32);
		PUSH_DATA (push, (bo->offset + TB_OFFSET));
		PUSH_DATA (push, 0);
		PUSH_DATA (push, 0x00000000);
		PUSH_DATA (push, 0x00000001);
		BEGIN_NVC0(push, NVC0_3D(CB_BIND(4)), 1);
		PUSH_DATA (push, 0x11);
		BEGIN_NVC0(push, SUBC_3D(0x2608), 1);
		PUSH_DATA (push, 1);
	}

	BEGIN_NVC0(push, NVC0_3D(VERTEX_QUARANTINE_ADDRESS_HIGH), 3);
	PUSH_DATA (push, (bo->offset + MISC_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + MISC_OFFSET));
	PUSH_DATA (push, 1);

	BEGIN_NVC0(push, NVC0_3D(CODE_ADDRESS_HIGH), 2);
	PUSH_DATA (push, (bo->offset + CODE_OFFSET) >> 32);
	PUSH_DATA (push, (bo->offset + CODE_OFFSET));
	if (pNv->Architecture < NV_ARCH_E0) {
		NVC0PushProgram(pNv, PVP_PASS, NVC0VP_Transform2);
		NVC0PushProgram(pNv, PFP_S, NVC0FP_Source);
		NVC0PushProgram(pNv, PFP_C, NVC0FP_Composite);
		NVC0PushProgram(pNv, PFP_CCA, NVC0FP_CAComposite);
		NVC0PushProgram(pNv, PFP_CCASA, NVC0FP_CACompositeSrcAlpha);
		NVC0PushProgram(pNv, PFP_S_A8, NVC0FP_Source_A8);
		NVC0PushProgram(pNv, PFP_C_A8, NVC0FP_Composite_A8);
		NVC0PushProgram(pNv, PFP_NV12, NVC0FP_NV12);

		BEGIN_NVC0(push, NVC0_3D(MEM_BARRIER), 1);
		PUSH_DATA (push, 0x1111);
	} else {
		NVC0PushProgram(pNv, PVP_PASS, NVE0VP_Transform2);
		NVC0PushProgram(pNv, PFP_S, NVE0FP_Source);
		NVC0PushProgram(pNv, PFP_C, NVE0FP_Composite);
		NVC0PushProgram(pNv, PFP_CCA, NVE0FP_CAComposite);
		NVC0PushProgram(pNv, PFP_CCASA, NVE0FP_CACompositeSrcAlpha);
		NVC0PushProgram(pNv, PFP_S_A8, NVE0FP_Source_A8);
		NVC0PushProgram(pNv, PFP_C_A8, NVE0FP_Composite_A8);
		NVC0PushProgram(pNv, PFP_NV12, NVE0FP_NV12);
	}

	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(1)), 4);
	PUSH_DATA (push, NVC0_3D_SP_SELECT_PROGRAM_VP_B |
			 NVC0_3D_SP_SELECT_ENABLE);
	PUSH_DATA (push, PVP_PASS);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 8);
	BEGIN_NVC0(push, NVC0_3D(VERT_COLOR_CLAMP_EN), 1);
	PUSH_DATA (push, 1);
	BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
	PUSH_DATA (push, 256);
	PUSH_DATA (push, (bo->offset + PVP_DATA) >> 32);
	PUSH_DATA (push, (bo->offset + PVP_DATA));
	BEGIN_NVC0(push, NVC0_3D(CB_BIND(0)), 1);
	PUSH_DATA (push, 0x01);

	BEGIN_NVC0(push, NVC0_3D(SP_SELECT(5)), 4);
	PUSH_DATA (push, NVC0_3D_SP_SELECT_PROGRAM_FP |
			 NVC0_3D_SP_SELECT_ENABLE);
	PUSH_DATA (push, PFP_S);
	PUSH_DATA (push, 0x00000000);
	PUSH_DATA (push, 8);
	BEGIN_NVC0(push, NVC0_3D(FRAG_COLOR_CLAMP_EN), 1);
	PUSH_DATA (push, 0x11111111);
	BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
	PUSH_DATA (push, 256);
	PUSH_DATA (push, (bo->offset + PFP_DATA) >> 32);
	PUSH_DATA (push, (bo->offset + PFP_DATA));
	BEGIN_NVC0(push, NVC0_3D(CB_BIND(4)), 1);
	PUSH_DATA (push, 0x01);

	return TRUE;
}

