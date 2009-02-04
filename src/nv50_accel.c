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
#include "nv50_accel.h"

Bool
NVAccelInitNV50TCL(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *tesla;
	unsigned class;
	int i;

	switch (pNv->NVArch & 0xf0) {
	case 0x50:
		class = 0x5097;
		break;
	case 0x80:
	case 0x90:
		class = 0x8297;
		break;
	case 0xa0:
	default:
		class = 0x8397;
		break;
	}

	if (!pNv->Nv3D) {
		if (nouveau_grobj_alloc(pNv->chan, Nv3D, class, &pNv->Nv3D))
			return FALSE;

		if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM, 0, 65536,
				   &pNv->tesla_scratch)) {
			nouveau_grobj_free(&pNv->Nv3D);
			return FALSE;
		}
	}
	tesla = pNv->Nv3D;

	BEGIN_RING(chan, tesla, 0x1558, 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, tesla, NV50TCL_DMA_NOTIFY, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, tesla, NV50TCL_DMA_UNK0(0), NV50TCL_DMA_UNK0__SIZE);
	for (i = 0; i < NV50TCL_DMA_UNK0__SIZE; i++)
		OUT_RING  (chan, pNv->chan->vram->handle);
	BEGIN_RING(chan, tesla, NV50TCL_DMA_UNK1(0), NV50TCL_DMA_UNK1__SIZE);
	for (i = 0; i < NV50TCL_DMA_UNK1__SIZE; i++)
		OUT_RING  (chan, pNv->chan->vram->handle);
	BEGIN_RING(chan, tesla, 0x121c, 1);
	OUT_RING  (chan, 1);

	BEGIN_RING(chan, tesla, 0x192c, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, 0x0f90, 1);
	OUT_RING  (chan, 1);

	BEGIN_RING(chan, tesla, 0x1234, 1);
	OUT_RING  (chan, 1);

	/*XXX: NFI - gets the oddball 0x1458 method working "properly" */
	BEGIN_RING(chan, tesla, 0x13bc, 1);
	OUT_RING  (chan, 0x54);

	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, PVP_OFFSET,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch, PVP_OFFSET,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, 0x00004000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, (3*4*2));	
	OUT_RING  (chan, 0x10000001);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000205);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000409);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x1000060d);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000811);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000a15);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000c19);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10000e1d);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10001021);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10001225);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x10001429);
	OUT_RING  (chan, 0x0423c788);
	OUT_RING  (chan, 0x1000162d);
	OUT_RING  (chan, 0x0423c789);

	BEGIN_RING(chan, tesla, NV50TCL_VP_ATTR_EN_0, 2);
	OUT_RING  (chan, 0x0000000f);
	OUT_RING  (chan, 0x000000ff);
	BEGIN_RING(chan, tesla, 0x16b8, 1);
	OUT_RING  (chan, 12);
	BEGIN_RING(chan, tesla, 0x16ac, 2);
	OUT_RING  (chan, 12);
	OUT_RING  (chan, 4);
	BEGIN_RING(chan, tesla, NV50TCL_VP_START_ID, 1);
	OUT_RING  (chan, 0);

	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_S, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_S, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 6);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82010200);
	OUT_RING  (chan, 0x82020204);
	OUT_RING  (chan, 0xf6400001);
	OUT_RING  (chan, 0x0000c785);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_C, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_C, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 16);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82030210);
	OUT_RING  (chan, 0x82040214);
	OUT_RING  (chan, 0x82010200);
	OUT_RING  (chan, 0x82020204);
	OUT_RING  (chan, 0xf6400001);
	OUT_RING  (chan, 0x0000c784);
	OUT_RING  (chan, 0xf0400211);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0xc0040000);
	OUT_RING  (chan, 0xc0040204);
	OUT_RING  (chan, 0xc0040408);
	OUT_RING  (chan, 0xc004060c);
	OUT_RING  (chan, 0x00000001);
	OUT_RING  (chan, 0x00000001);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_CCA, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch,
			 PFP_OFFSET + PFP_CCA, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 16);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82030210);
	OUT_RING  (chan, 0x82040214);
	OUT_RING  (chan, 0x82010200);
	OUT_RING  (chan, 0x82020204);
	OUT_RING  (chan, 0xf6400001);
	OUT_RING  (chan, 0x0000c784);
	OUT_RING  (chan, 0xf6400211);
	OUT_RING  (chan, 0x0000c784);
	OUT_RING  (chan, 0xc0040000);
	OUT_RING  (chan, 0xc0050204);
	OUT_RING  (chan, 0xc0060408);
	OUT_RING  (chan, 0xc007060c);
	OUT_RING  (chan, 0x00000001);
	OUT_RING  (chan, 0x00000001);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_CCASA,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_CCASA,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 16);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82030200);
	OUT_RING  (chan, 0x82040204);
	OUT_RING  (chan, 0x82010210);
	OUT_RING  (chan, 0x82020214);
	OUT_RING  (chan, 0xf6400201);
	OUT_RING  (chan, 0x0000c784);
	OUT_RING  (chan, 0xf0400011);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0xc0040000);
	OUT_RING  (chan, 0xc0040204);
	OUT_RING  (chan, 0xc0040408);
	OUT_RING  (chan, 0xc004060c);
	OUT_RING  (chan, 0x00000001);
	OUT_RING  (chan, 0x00000001);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_S_A8,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_S_A8,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 10);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82010200);
	OUT_RING  (chan, 0x82020204);
	OUT_RING  (chan, 0xf0400001);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0x10008004);
	OUT_RING  (chan, 0x10008008);
	OUT_RING  (chan, 0x1000000d);
	OUT_RING  (chan, 0x0403c781);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_C_A8,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_C_A8,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 15);
	OUT_RING  (chan, 0x80000000);
	OUT_RING  (chan, 0x90000004);
	OUT_RING  (chan, 0x82030208);
	OUT_RING  (chan, 0x8204020c);
	OUT_RING  (chan, 0x82010200);
	OUT_RING  (chan, 0x82020204);
	OUT_RING  (chan, 0xf0400001);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0xf0400209);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0xc002000c);
	OUT_RING  (chan, 0x10008600);
	OUT_RING  (chan, 0x10008604);
	OUT_RING  (chan, 0x10000609);
	OUT_RING  (chan, 0x0403c781);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DEF_ADDRESS_HIGH, 3);
	OUT_RELOCh(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_NV12,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RELOCl(chan, pNv->tesla_scratch, PFP_OFFSET + PFP_NV12,
			 NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (chan, (0 << NV50TCL_CB_DEF_SET_BUFFER_SHIFT) | 0x4000);
	BEGIN_RING(chan, tesla, NV50TCL_CB_ADDR, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, tesla, NV50TCL_CB_DATA(0) | 0x40000000, 34);
	OUT_RING  (chan, 0x80000008);
	OUT_RING  (chan, 0x90000408);
	OUT_RING  (chan, 0x80010400);
	OUT_RING  (chan, 0x80020404);
	OUT_RING  (chan, 0xf0400001);
	OUT_RING  (chan, 0x00008784);
	OUT_RING  (chan, 0xc0080001);
	OUT_RING  (chan, 0x03f9507f);
	OUT_RING  (chan, 0xb013000d);
	OUT_RING  (chan, 0x0bf5ee3b);
	OUT_RING  (chan, 0xb02f0011);
	OUT_RING  (chan, 0x03f078ff);
	OUT_RING  (chan, 0xb0220015);
	OUT_RING  (chan, 0x0bf8a677);
	OUT_RING  (chan, 0x80030400);
	OUT_RING  (chan, 0x80040404);
	OUT_RING  (chan, 0xf0400201);
	OUT_RING  (chan, 0x0000c784);
	OUT_RING  (chan, 0xc0160009);
	OUT_RING  (chan, 0x0bec890f);
	OUT_RING  (chan, 0xb0000411);
	OUT_RING  (chan, 0x00010780);
	OUT_RING  (chan, 0xc0070009);
	OUT_RING  (chan, 0x0400116b);
	OUT_RING  (chan, 0xc02d0201);
	OUT_RING  (chan, 0x03fcc433);
	OUT_RING  (chan, 0xc0370205);
	OUT_RING  (chan, 0x0bf501a3);
	OUT_RING  (chan, 0xb0000001);
	OUT_RING  (chan, 0x0000c780);
	OUT_RING  (chan, 0xb0000205);
	OUT_RING  (chan, 0x00010780);
	OUT_RING  (chan, 0xb0000409);
	OUT_RING  (chan, 0x00014781);
 
	BEGIN_RING(chan, tesla, 0x16bc, 2);
	OUT_RING  (chan, 0x03020100);
	OUT_RING  (chan, 0x09080504);
	BEGIN_RING(chan, tesla, 0x1520, 1);
	OUT_RING  (chan, 0x00000000);
	BEGIN_RING(chan, tesla, 0x1988, 2);
	OUT_RING  (chan, 0x08070407);
	OUT_RING  (chan, 0x00000008);

	BEGIN_RING(chan, tesla, NV50TCL_VIEWPORT_HORIZ, 2);
	OUT_RING  (chan, 8192 << NV50TCL_VIEWPORT_HORIZ_W_SHIFT);
	OUT_RING  (chan, 8192 << NV50TCL_VIEWPORT_VERT_H_SHIFT);
	BEGIN_RING(chan, tesla, NV50TCL_SCISSOR_HORIZ, 2);
	OUT_RING  (chan, 8192 << NV50TCL_SCISSOR_HORIZ_R_SHIFT);
	OUT_RING  (chan, 8192 << NV50TCL_SCISSOR_VERT_B_SHIFT);
	BEGIN_RING(chan, tesla, 0x0ff4, 2);
	OUT_RING  (chan, 8192 << NV50TCL_UNKFF4_W_SHIFT);
	OUT_RING  (chan, 8192 << NV50TCL_UNKFF8_H_SHIFT);

	return TRUE;
}

