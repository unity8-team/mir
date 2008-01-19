/***************************************************************************\
|*                                                                           *|
|*       Copyright 1993-2003 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-2003 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_hw.c,v 1.21 2006/06/16 00:19:33 mvojkovi Exp $ */

#include "nv_include.h"
#include "nv_local.h"
#include "compiler.h"

/* Reminder: Do NOT use these function for the randr-1.2 codepath. */
uint8_t nvReadVGA(NVPtr pNv, uint8_t index)
{
	volatile const uint8_t *ptr = pNv->cur_head ? pNv->PCIO1 : pNv->PCIO0;
	assert(pNv->randr12_enable == FALSE);
	VGA_WR08(ptr, 0x03D4, index);
	return VGA_RD08(ptr, 0x03D5);
}
void nvWriteVGA(NVPtr pNv, uint8_t index, uint8_t data)
{
	volatile const uint8_t *ptr = pNv->cur_head ? pNv->PCIO1 : pNv->PCIO0;
	assert(pNv->randr12_enable == FALSE);
	VGA_WR08(ptr, 0x03D4, index);
	VGA_WR08(ptr, 0x03D5, data);
}

uint32_t nvReadRAMDAC(NVPtr pNv, uint8_t head, uint32_t reg)
{
	volatile const void *ptr = head ? pNv->PRAMDAC1 : pNv->PRAMDAC0;
	assert(pNv->randr12_enable == FALSE);
	DDXMMIOH("nvReadRamdac: head %d reg %08x val %08x\n", head, reg + NV_PRAMDAC0_OFFSET + (head ? NV_PRAMDAC0_SIZE : 0), (uint32_t)MMIO_IN32(ptr, reg));
	return MMIO_IN32(ptr, reg);
}

void nvWriteRAMDAC(NVPtr pNv, uint8_t head, uint32_t reg, uint32_t val)
{
	volatile const void *ptr = head ? pNv->PRAMDAC1 : pNv->PRAMDAC0;
	assert(pNv->randr12_enable == FALSE);
	DDXMMIOH("nvWriteRamdac: head %d reg %08x val %08x\n", head, reg + NV_PRAMDAC0_OFFSET + (head ? NV_PRAMDAC0_SIZE : 0), val);
	MMIO_OUT32(ptr, reg, val);
}

uint32_t nvReadCRTC(NVPtr pNv, uint8_t head, uint32_t reg)
{
	volatile const void *ptr = head ? pNv->PCRTC1 : pNv->PCRTC0;
	assert(pNv->randr12_enable == FALSE);
	DDXMMIOH("nvReadCRTC: head %d reg %08x val %08x\n", head, reg + NV_PCRTC0_OFFSET + (head ? NV_PCRTC0_SIZE : 0), (uint32_t)MMIO_IN32(ptr, reg));
	return MMIO_IN32(ptr, reg);
}

void nvWriteCRTC(NVPtr pNv, uint8_t head, uint32_t reg, uint32_t val)
{
	volatile const void *ptr = head ? pNv->PCRTC1 : pNv->PCRTC0;
	assert(pNv->randr12_enable == FALSE);
	DDXMMIOH("nvWriteCRTC: head %d reg %08x val %08x\n", head, reg + NV_PCRTC0_OFFSET + (head ? NV_PCRTC0_SIZE : 0), val);
	MMIO_OUT32(ptr, reg, val);
}

void NVLockUnlock (
    NVPtr pNv,
    Bool  Lock
)
{
    CARD8 cr11;

    nvWriteVGA(pNv, NV_VGA_CRTCX_LOCK, Lock ? 0x99 : 0x57 );

    cr11 = nvReadVGA(pNv, NV_VGA_CRTCX_VSYNCE);
    if(Lock) cr11 |= 0x80;
    else cr11 &= ~0x80;
    nvWriteVGA(pNv, NV_VGA_CRTCX_VSYNCE, cr11);
}

int NVShowHideCursor (
    NVPtr pNv,
    int   ShowHide
)
{
    int current = pNv->CurrentState->cursor1;

    pNv->CurrentState->cursor1 = (pNv->CurrentState->cursor1 & 0xFE) |
                                 (ShowHide & 0x01);

    nvWriteVGA(pNv, NV_VGA_CRTCX_CURCTL1, pNv->CurrentState->cursor1);

    if(pNv->Architecture == NV_ARCH_40) {  /* HW bug */
       volatile CARD32 curpos = nvReadCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS);
       nvWriteCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS, curpos);
    }

    return (current & 0x01);
}

/****************************************************************************\
*                                                                            *
* The video arbitration routines calculate some "magic" numbers.  Fixes      *
* the snow seen when accessing the framebuffer without it.                   *
* It just works (I hope).                                                    *
*                                                                            *
\****************************************************************************/

typedef struct {
  int graphics_lwm;
  int video_lwm;
  int graphics_burst_size;
  int video_burst_size;
  int valid;
} nv4_fifo_info;

typedef struct {
  int pclk_khz;
  int mclk_khz;
  int nvclk_khz;
  char mem_page_miss;
  char mem_latency;
  int memory_width;
  char enable_video;
  char gr_during_vid;
  char pix_bpp;
  char mem_aligned;
  char enable_mp;
} nv4_sim_state;

typedef struct {
  int graphics_lwm;
  int video_lwm;
  int graphics_burst_size;
  int video_burst_size;
  int valid;
} nv10_fifo_info;

typedef struct {
  uint32_t pclk_khz;
  uint32_t mclk_khz;
  uint32_t nvclk_khz;
  uint8_t mem_page_miss;
  uint8_t mem_latency;
  uint32_t memory_type;
  uint32_t memory_width;
  uint8_t enable_video;
  uint8_t gr_during_vid;
  uint8_t pix_bpp;
  uint8_t mem_aligned;
  uint8_t enable_mp;
} nv10_sim_state;


static void nvGetClocks(NVPtr pNv, unsigned int *MClk, unsigned int *NVClk)
{
	unsigned int pll, N, M, MB, NB, P;

    if(pNv->Architecture >= NV_ARCH_40) {
	Bool VCO2_off = FALSE;
       pll = nvReadMC(pNv, 0x4020);
       P = (pll >> 16) & 0x07;
	/* There seem to be 2 (redundant?) switches to turn VCO2 off. */
	if (pll & (1 << 8))
		VCO2_off = TRUE;
	if (!(pll & (1 << 30))) 
		VCO2_off = TRUE;
       pll = nvReadMC(pNv, 0x4024);
       M = pll & 0xFF;
       N = (pll >> 8) & 0xFF;
	if (VCO2_off) {
		MB = 1;
		NB = 1;
	} else {
		MB = (pll >> 16) & 0xFF;
		NB = (pll >> 24) & 0xFF;
	}
	if (!MB || !NB) {
		ErrorF("Something wrong with MPLL VCO2 settings, ignoring VCO2.\n");
		MB = 1;
		NB = 1;
	}

       *MClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;

	VCO2_off = FALSE; /* reset */

       pll = nvReadMC(pNv, 0x4000);
       P = (pll >> 16) & 0x07;
	/* There seem to be 2 (redundant?) switches to turn VCO2 off. */
	if (pll & (1 << 8))
		VCO2_off = TRUE;
	if (!(pll & (1 << 30))) 
		VCO2_off = TRUE;
       pll = nvReadMC(pNv, 0x4004);
       M = pll & 0xFF;
       N = (pll >> 8) & 0xFF;
	if (VCO2_off) {
		MB = 1;
		NB = 1;
	} else {
		MB = (pll >> 16) & 0xFF;
		NB = (pll >> 24) & 0xFF;
	}
	if (!MB || !NB) {
		ErrorF("Something wrong with NVPLL VCO2 settings, ignoring VCO2\n");
		MB = 1;
		NB = 1;
	}

       *NVClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;
    } else
    if(pNv->twoStagePLL) {
       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_MPLL);
       M = pll & 0xFF;
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_MPLL_B);
       if(pll & 0x80000000) {
           MB = pll & 0xFF; 
           NB = (pll >> 8) & 0xFF;
       } else {
           MB = 1;
           NB = 1;
       }
       *MClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;

       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_NVPLL);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_NVPLL_B);
       if(pll & 0x80000000) {
           MB = pll & 0xFF;
           NB = (pll >> 8) & 0xFF;
       } else {
           MB = 1;
           NB = 1;
       }
       *NVClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;
    } else 
    if(((pNv->Chipset & 0x0ff0) == CHIPSET_NV30) ||
       ((pNv->Chipset & 0x0ff0) == CHIPSET_NV35))
    {
       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_MPLL);
       M = pll & 0x0F; 
       N = (pll >> 8) & 0xFF;
       P = (pll >> 16) & 0x07;
       if(pll & 0x00000080) {
           MB = (pll >> 4) & 0x07;     
           NB = (pll >> 19) & 0x1f;
       } else {
           MB = 1;
           NB = 1;
       }
       *MClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;

       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_NVPLL);
       M = pll & 0x0F;
       N = (pll >> 8) & 0xFF;
       P = (pll >> 16) & 0x07;
       if(pll & 0x00000080) {
           MB = (pll >> 4) & 0x07;
           NB = (pll >> 19) & 0x1f;
       } else {
           MB = 1;
           NB = 1;
       }
       *NVClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;
    } else {
       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_MPLL);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       *MClk = (N * pNv->CrystalFreqKHz / M) >> P;

       pll = nvReadRAMDAC0(pNv, NV_RAMDAC_NVPLL);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       *NVClk = (N * pNv->CrystalFreqKHz / M) >> P;
    }

#if 0
    ErrorF("NVClock = %i MHz, MEMClock = %i MHz\n", *NVClk/1000, *MClk/1000);
#endif
}


void nv4CalcArbitration (
    nv4_fifo_info *fifo,
    nv4_sim_state *arb
)
{
    int data, pagemiss, cas,width, video_enable, bpp;
    int nvclks, mclks, pclks, vpagemiss, crtpagemiss, vbs;
    int found, mclk_extra, mclk_loop, cbs, m1, p1;
    int mclk_freq, pclk_freq, nvclk_freq, mp_enable;
    int us_m, us_n, us_p, video_drain_rate, crtc_drain_rate;
    int vpm_us, us_video, vlwm, video_fill_us, cpm_us, us_crt,clwm;

    fifo->valid = 1;
    pclk_freq = arb->pclk_khz;
    mclk_freq = arb->mclk_khz;
    nvclk_freq = arb->nvclk_khz;
    pagemiss = arb->mem_page_miss;
    cas = arb->mem_latency;
    width = arb->memory_width >> 6;
    video_enable = arb->enable_video;
    bpp = arb->pix_bpp;
    mp_enable = arb->enable_mp;
    clwm = 0;
    vlwm = 0;
    cbs = 128;
    pclks = 2;
    nvclks = 2;
    nvclks += 2;
    nvclks += 1;
    mclks = 5;
    mclks += 3;
    mclks += 1;
    mclks += cas;
    mclks += 1;
    mclks += 1;
    mclks += 1;
    mclks += 1;
    mclk_extra = 3;
    nvclks += 2;
    nvclks += 1;
    nvclks += 1;
    nvclks += 1;
    if (mp_enable)
        mclks+=4;
    nvclks += 0;
    pclks += 0;
    found = 0;
    vbs = 0;
    while (found != 1)
    {
        fifo->valid = 1;
        found = 1;
        mclk_loop = mclks+mclk_extra;
        us_m = mclk_loop *1000*1000 / mclk_freq;
        us_n = nvclks*1000*1000 / nvclk_freq;
        us_p = nvclks*1000*1000 / pclk_freq;
        if (video_enable)
        {
            video_drain_rate = pclk_freq * 2;
            crtc_drain_rate = pclk_freq * bpp/8;
            vpagemiss = 2;
            vpagemiss += 1;
            crtpagemiss = 2;
            vpm_us = (vpagemiss * pagemiss)*1000*1000/mclk_freq;
            if (nvclk_freq * 2 > mclk_freq * width)
                video_fill_us = cbs*1000*1000 / 16 / nvclk_freq ;
            else
                video_fill_us = cbs*1000*1000 / (8 * width) / mclk_freq;
            us_video = vpm_us + us_m + us_n + us_p + video_fill_us;
            vlwm = us_video * video_drain_rate/(1000*1000);
            vlwm++;
            vbs = 128;
            if (vlwm > 128) vbs = 64;
            if (vlwm > (256-64)) vbs = 32;
            if (nvclk_freq * 2 > mclk_freq * width)
                video_fill_us = vbs *1000*1000/ 16 / nvclk_freq ;
            else
                video_fill_us = vbs*1000*1000 / (8 * width) / mclk_freq;
            cpm_us = crtpagemiss  * pagemiss *1000*1000/ mclk_freq;
            us_crt =
            us_video
            +video_fill_us
            +cpm_us
            +us_m + us_n +us_p
            ;
            clwm = us_crt * crtc_drain_rate/(1000*1000);
            clwm++;
        }
        else
        {
            crtc_drain_rate = pclk_freq * bpp/8;
            crtpagemiss = 2;
            crtpagemiss += 1;
            cpm_us = crtpagemiss  * pagemiss *1000*1000/ mclk_freq;
            us_crt =  cpm_us + us_m + us_n + us_p ;
            clwm = us_crt * crtc_drain_rate/(1000*1000);
            clwm++;
        }
        m1 = clwm + cbs - 512;
        p1 = m1 * pclk_freq / mclk_freq;
        p1 = p1 * bpp / 8;
        if ((p1 < m1) && (m1 > 0))
        {
            fifo->valid = 0;
            found = 0;
            if (mclk_extra ==0)   found = 1;
            mclk_extra--;
        }
        else if (video_enable)
        {
            if ((clwm > 511) || (vlwm > 255))
            {
                fifo->valid = 0;
                found = 0;
                if (mclk_extra ==0)   found = 1;
                mclk_extra--;
            }
        }
        else
        {
            if (clwm > 519)
            {
                fifo->valid = 0;
                found = 0;
                if (mclk_extra ==0)   found = 1;
                mclk_extra--;
            }
        }
        if (clwm < 384) clwm = 384;
        if (vlwm < 128) vlwm = 128;
        data = (int)(clwm);
        fifo->graphics_lwm = data;
        fifo->graphics_burst_size = 128;
        data = (int)((vlwm+15));
        fifo->video_lwm = data;
        fifo->video_burst_size = vbs;
    }
}

void nv4UpdateArbitrationSettings (
    unsigned      VClk, 
    unsigned      pixelDepth, 
    unsigned     *burst,
    unsigned     *lwm,
    NVPtr        pNv
)
{
    nv4_fifo_info fifo_data;
    nv4_sim_state sim_data;
    unsigned int MClk, NVClk, cfg1;

    nvGetClocks(pNv, &MClk, &NVClk);

    cfg1 = nvReadFB(pNv, NV_PFB_CFG1);
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 0;
    sim_data.enable_mp      = 0;
    sim_data.memory_width   = (nvReadEXTDEV(pNv, 0x0000) & 0x10) ? 128 : 64;
    sim_data.mem_latency    = (char)cfg1 & 0x0F;
    sim_data.mem_aligned    = 1;
    sim_data.mem_page_miss  = (char)(((cfg1 >> 4) &0x0F) + ((cfg1 >> 31) & 0x01));
    sim_data.gr_during_vid  = 0;
    sim_data.pclk_khz       = VClk;
    sim_data.mclk_khz       = MClk;
    sim_data.nvclk_khz      = NVClk;
    nv4CalcArbitration(&fifo_data, &sim_data);
    if (fifo_data.valid)
    {
        int  b = fifo_data.graphics_burst_size >> 4;
        *burst = 0;
        while (b >>= 1) (*burst)++;
        *lwm   = fifo_data.graphics_lwm >> 3;
    }
}

void nv10CalcArbitration (
    nv10_fifo_info *fifo,
    nv10_sim_state *arb
)
{
    int data, pagemiss, width, video_enable, bpp;
    int nvclks, mclks, pclks, vpagemiss, crtpagemiss;
    int nvclk_fill;
    int found, mclk_extra, mclk_loop, cbs, m1;
    int mclk_freq, pclk_freq, nvclk_freq, mp_enable;
    int us_m, us_m_min, us_n, us_p, crtc_drain_rate;
    int vus_m;
    int vpm_us, us_video, cpm_us, us_crt,clwm;
    int clwm_rnd_down;
    int m2us, us_pipe_min, p1clk, p2;
    int min_mclk_extra;
    int us_min_mclk_extra;

    fifo->valid = 1;
    pclk_freq = arb->pclk_khz; /* freq in KHz */
    mclk_freq = arb->mclk_khz;
    nvclk_freq = arb->nvclk_khz;
    pagemiss = arb->mem_page_miss;
    width = arb->memory_width/64;
    video_enable = arb->enable_video;
    bpp = arb->pix_bpp;
    mp_enable = arb->enable_mp;
    clwm = 0;

    cbs = 512;

    pclks = 4; /* lwm detect. */

    nvclks = 3; /* lwm -> sync. */
    nvclks += 2; /* fbi bus cycles (1 req + 1 busy) */

    mclks  = 1;   /* 2 edge sync.  may be very close to edge so just put one. */

    mclks += 1;   /* arb_hp_req */
    mclks += 5;   /* ap_hp_req   tiling pipeline */

    mclks += 2;    /* tc_req     latency fifo */
    mclks += 2;    /* fb_cas_n_  memory request to fbio block */
    mclks += 7;    /* sm_d_rdv   data returned from fbio block */

    /* fb.rd.d.Put_gc   need to accumulate 256 bits for read */
    if (arb->memory_type == 0)
      if (arb->memory_width == 64) /* 64 bit bus */
        mclks += 4;
      else
        mclks += 2;
    else
      if (arb->memory_width == 64) /* 64 bit bus */
        mclks += 2;
      else
        mclks += 1;

    if ((!video_enable) && (arb->memory_width == 128))
    {  
      mclk_extra = (bpp == 32) ? 31 : 42; /* Margin of error */
      min_mclk_extra = 17;
    }
    else
    {
      mclk_extra = (bpp == 32) ? 8 : 4; /* Margin of error */
      /* mclk_extra = 4; */ /* Margin of error */
      min_mclk_extra = 18;
    }

    nvclks += 1; /* 2 edge sync.  may be very close to edge so just put one. */
    nvclks += 1; /* fbi_d_rdv_n */
    nvclks += 1; /* Fbi_d_rdata */
    nvclks += 1; /* crtfifo load */

    if(mp_enable)
      mclks+=4; /* Mp can get in with a burst of 8. */
    /* Extra clocks determined by heuristics */

    nvclks += 0;
    pclks += 0;
    found = 0;
    while(found != 1) {
      fifo->valid = 1;
      found = 1;
      mclk_loop = mclks+mclk_extra;
      us_m = mclk_loop *1000*1000 / mclk_freq; /* Mclk latency in us */
      us_m_min = mclks * 1000*1000 / mclk_freq; /* Minimum Mclk latency in us */
      us_min_mclk_extra = min_mclk_extra *1000*1000 / mclk_freq;
      us_n = nvclks*1000*1000 / nvclk_freq;/* nvclk latency in us */
      us_p = pclks*1000*1000 / pclk_freq;/* nvclk latency in us */
      us_pipe_min = us_m_min + us_n + us_p;

      vus_m = mclk_loop *1000*1000 / mclk_freq; /* Mclk latency in us */

      if(video_enable) {
        crtc_drain_rate = pclk_freq * bpp/8; /* MB/s */

        vpagemiss = 1; /* self generating page miss */
        vpagemiss += 1; /* One higher priority before */

        crtpagemiss = 2; /* self generating page miss */
        if(mp_enable)
            crtpagemiss += 1; /* if MA0 conflict */

        vpm_us = (vpagemiss * pagemiss)*1000*1000/mclk_freq;

        us_video = vpm_us + vus_m; /* Video has separate read return path */

        cpm_us = crtpagemiss  * pagemiss *1000*1000/ mclk_freq;
        us_crt =
          us_video  /* Wait for video */
          +cpm_us /* CRT Page miss */
          +us_m + us_n +us_p /* other latency */
          ;

        clwm = us_crt * crtc_drain_rate/(1000*1000);
        clwm++; /* fixed point <= float_point - 1.  Fixes that */
      } else {
        crtc_drain_rate = pclk_freq * bpp/8; /* bpp * pclk/8 */

        crtpagemiss = 1; /* self generating page miss */
        crtpagemiss += 1; /* MA0 page miss */
        if(mp_enable)
            crtpagemiss += 1; /* if MA0 conflict */
        cpm_us = crtpagemiss  * pagemiss *1000*1000/ mclk_freq;
        us_crt =  cpm_us + us_m + us_n + us_p ;
        clwm = us_crt * crtc_drain_rate/(1000*1000);
        clwm++; /* fixed point <= float_point - 1.  Fixes that */

          /* Finally, a heuristic check when width == 64 bits */
          if(width == 1){
              nvclk_fill = nvclk_freq * 8;
              if(crtc_drain_rate * 100 >= nvclk_fill * 102)
                      clwm = 0xfff; /*Large number to fail */

              else if(crtc_drain_rate * 100  >= nvclk_fill * 98) {
                  clwm = 1024;
                  cbs = 512;
              }
          }
      }


      /*
        Overfill check:

        */

      clwm_rnd_down = ((int)clwm/8)*8;
      if (clwm_rnd_down < clwm)
          clwm += 8;

      m1 = clwm + cbs -  1024; /* Amount of overfill */
      m2us = us_pipe_min + us_min_mclk_extra;

      /* pclk cycles to drain */
      p1clk = m2us * pclk_freq/(1000*1000); 
      p2 = p1clk * bpp / 8; /* bytes drained. */

      if((p2 < m1) && (m1 > 0)) {
          fifo->valid = 0;
          found = 0;
          if(min_mclk_extra == 0)   {
            if(cbs <= 32) {
              found = 1; /* Can't adjust anymore! */
            } else {
              cbs = cbs/2;  /* reduce the burst size */
            }
          } else {
            min_mclk_extra--;
          }
      } else {
        if (clwm > 1023){ /* Have some margin */
          fifo->valid = 0;
          found = 0;
          if(min_mclk_extra == 0)   
              found = 1; /* Can't adjust anymore! */
          else 
              min_mclk_extra--;
        }
      }

      if(clwm < (1024-cbs+8)) clwm = 1024-cbs+8;
      data = (int)(clwm);
      /*  printf("CRT LWM: %f bytes, prog: 0x%x, bs: 256\n", clwm, data ); */
      fifo->graphics_lwm = data;   fifo->graphics_burst_size = cbs;

      fifo->video_lwm = 1024;  fifo->video_burst_size = 512;
    }
}

void nv10UpdateArbitrationSettings (
    unsigned      VClk, 
    unsigned      pixelDepth, 
    unsigned     *burst,
    unsigned     *lwm,
    NVPtr        pNv
)
{
    nv10_fifo_info fifo_data;
    nv10_sim_state sim_data;
    unsigned int MClk, NVClk, cfg1;

    nvGetClocks(pNv, &MClk, &NVClk);

    cfg1 = nvReadFB(pNv, NV_PFB_CFG1);
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 1;
    sim_data.enable_mp      = 0;
    sim_data.memory_type    = (nvReadFB(pNv, NV_PFB_CFG0) & 0x01) ? 1 : 0;
    sim_data.memory_width   = (nvReadEXTDEV(pNv, 0x0000) & 0x10) ? 128 : 64;
    sim_data.mem_latency    = (char)cfg1 & 0x0F;
    sim_data.mem_aligned    = 1;
    sim_data.mem_page_miss  = (char)(((cfg1>>4) &0x0F) + ((cfg1>>31) & 0x01));
    sim_data.gr_during_vid  = 0;
    sim_data.pclk_khz       = VClk;
    sim_data.mclk_khz       = MClk;
    sim_data.nvclk_khz      = NVClk;
    nv10CalcArbitration(&fifo_data, &sim_data);
    if (fifo_data.valid) {
        int  b = fifo_data.graphics_burst_size >> 4;
        *burst = 0;
        while (b >>= 1) (*burst)++;
        *lwm   = fifo_data.graphics_lwm >> 3;
    }
}


void nv30UpdateArbitrationSettings (NVPtr pNv,
				    unsigned     *burst,
				    unsigned     *lwm)   
{
    unsigned int MClk, NVClk;
    unsigned int fifo_size, burst_size, graphics_lwm;

    fifo_size = 2048;
    burst_size = 512;
    graphics_lwm = fifo_size - burst_size;

    nvGetClocks(pNv, &MClk, &NVClk);
    
    *burst = 0;
    burst_size >>= 5;
    while(burst_size >>= 1) (*burst)++;
    *lwm = graphics_lwm >> 3;
}

#ifdef XSERVER_LIBPCIACCESS

struct pci_device GetDeviceByPCITAG(uint32_t bus, uint32_t dev, uint32_t func)
{
	const struct pci_slot_match match[] = { {0, bus, dev, func, 0} };
	struct pci_device_iterator *iterator;
	struct pci_device *device;
	
	/* assume one device to exist */
	iterator = pci_slot_match_iterator_create(match);
	device = pci_device_next(iterator);

	return *device;
}

#endif /* XSERVER_LIBPCIACCESS */

void nForceUpdateArbitrationSettings (unsigned VClk,
				      unsigned      pixelDepth,
				      unsigned     *burst,
				      unsigned     *lwm,
				      NVPtr        pNv
)
{
    nv10_fifo_info fifo_data;
    nv10_sim_state sim_data;
    unsigned int M, N, P, pll, MClk, NVClk, memctrl;

#ifdef XSERVER_LIBPCIACCESS
	struct pci_device tmp;
#endif /* XSERVER_LIBPCIACCESS */

    if((pNv->Chipset & 0x0FF0) == CHIPSET_NFORCE) {
       unsigned int uMClkPostDiv;

#ifdef XSERVER_LIBPCIACCESS
	tmp = GetDeviceByPCITAG(0, 0, 3);
	PCI_DEV_READ_LONG(&tmp, 0x6C, &(uMClkPostDiv));
	uMClkPostDiv = (uMClkPostDiv >> 8) & 0xf;
#else
	uMClkPostDiv = (pciReadLong(pciTag(0, 0, 3), 0x6C) >> 8) & 0xf;
#endif /* XSERVER_LIBPCIACCESS */
       if(!uMClkPostDiv) uMClkPostDiv = 4; 
       MClk = 400000 / uMClkPostDiv;
    } else {
#ifdef XSERVER_LIBPCIACCESS
	tmp = GetDeviceByPCITAG(0, 0, 5);
	PCI_DEV_READ_LONG(&tmp, 0x4C, &(MClk));
	MClk /= 1000;
#else
	MClk = pciReadLong(pciTag(0, 0, 5), 0x4C) / 1000;
#endif /* XSERVER_LIBPCIACCESS */
    }

    pll = nvReadRAMDAC0(pNv, NV_RAMDAC_NVPLL);
    M = (pll >> 0)  & 0xFF; N = (pll >> 8)  & 0xFF; P = (pll >> 16) & 0x0F;
    NVClk  = (N * pNv->CrystalFreqKHz / M) >> P;
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 0;
    sim_data.enable_mp      = 0;
#ifdef XSERVER_LIBPCIACCESS
	tmp = GetDeviceByPCITAG(0, 0, 1);
	PCI_DEV_READ_LONG(&tmp, 0x7C, &(sim_data.memory_type));
	sim_data.memory_type = (sim_data.memory_type >> 12) & 1;
#else
	sim_data.memory_type = (pciReadLong(pciTag(0, 0, 1), 0x7C) >> 12) & 1;
#endif /* XSERVER_LIBPCIACCESS */
    sim_data.memory_width   = 64;

#ifdef XSERVER_LIBPCIACCESS
	/* This offset is 0, is this even usefull? */
	tmp = GetDeviceByPCITAG(0, 0, 3);
	PCI_DEV_READ_LONG(&tmp, 0x00, &(memctrl));
	memctrl >>= 16;
#else
	memctrl = pciReadLong(pciTag(0, 0, 3), 0x00) >> 16;
#endif /* XSERVER_LIBPCIACCESS */

    if((memctrl == 0x1A9) || (memctrl == 0x1AB) || (memctrl == 0x1ED)) {
        uint32_t dimm[3];
#ifdef XSERVER_LIBPCIACCESS
	tmp = GetDeviceByPCITAG(0, 0, 2);
	PCI_DEV_READ_LONG(&tmp, 0x40, &(dimm[0]));
	PCI_DEV_READ_LONG(&tmp, 0x44, &(dimm[1]));
	PCI_DEV_READ_LONG(&tmp, 0x48, &(dimm[2]));
	int i;
	for (i = 0; i < 3; i++) {
		dimm[i] = (dimm[i] >> 8) & 0x4F;
	}
#else
	dimm[0] = (pciReadLong(pciTag(0, 0, 2), 0x40) >> 8) & 0x4F;
	dimm[1] = (pciReadLong(pciTag(0, 0, 2), 0x44) >> 8) & 0x4F;
	dimm[2] = (pciReadLong(pciTag(0, 0, 2), 0x48) >> 8) & 0x4F;
#endif

        if((dimm[0] + dimm[1]) != dimm[2]) {
             ErrorF("WARNING: "
              "your nForce DIMMs are not arranged in optimal banks!\n");
        } 
    }

    sim_data.mem_latency    = 3;
    sim_data.mem_aligned    = 1;
    sim_data.mem_page_miss  = 10;
    sim_data.gr_during_vid  = 0;
    sim_data.pclk_khz       = VClk;
    sim_data.mclk_khz       = MClk;
    sim_data.nvclk_khz      = NVClk;
    nv10CalcArbitration(&fifo_data, &sim_data);
    if (fifo_data.valid)
    {
        int  b = fifo_data.graphics_burst_size >> 4;
        *burst = 0;
        while (b >>= 1) (*burst)++;
        *lwm   = fifo_data.graphics_lwm >> 3;
    }
}


/****************************************************************************\
*                                                                            *
*                          RIVA Mode State Routines                          *
*                                                                            *
\****************************************************************************/

/*
 * Calculate the Video Clock parameters for the PLL.
 */
static void CalcVClock (
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    NVPtr        pNv
)
{
    unsigned lowM, highM;
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;
    
    DeltaOld = 0xFFFFFFFF;

    VClk = (unsigned)clockIn;
    
    if (pNv->CrystalFreqKHz == 13500) {
        lowM  = 7;
        highM = 13;
    } else {
        lowM  = 8;
        highM = 14;
    }

    for (P = 0; P <= 4; P++) {
        Freq = VClk << P;
        if ((Freq >= 128000) && (Freq <= 350000)) {
            for (M = lowM; M <= highM; M++) {
                N = ((VClk << P) * M) / pNv->CrystalFreqKHz;
                if(N <= 255) {
                    Freq = ((pNv->CrystalFreqKHz * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
                    if (DeltaNew < DeltaOld) {
                        *pllOut   = (P << 16) | (N << 8) | M;
                        *clockOut = Freq;
                        DeltaOld  = DeltaNew;
                    }
                }
            }
        }
    }
}

static void CalcVClock2Stage (
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    CARD32         *pllBOut,
    NVPtr        pNv
)
{
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;

    DeltaOld = 0xFFFFFFFF;

    *pllBOut = 0x80000401;  /* fixed at x4 for now */

    VClk = (unsigned)clockIn;

    for (P = 0; P <= 6; P++) {
        Freq = VClk << P;
        if ((Freq >= 400000) && (Freq <= 1000000)) {
            for (M = 1; M <= 13; M++) {
                N = ((VClk << P) * M) / (pNv->CrystalFreqKHz << 2);
                if((N >= 5) && (N <= 255)) {
                    Freq = (((pNv->CrystalFreqKHz << 2) * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
                    if (DeltaNew < DeltaOld) {
                        *pllOut   = (P << 16) | (N << 8) | M;
                        *clockOut = Freq;
                        DeltaOld  = DeltaNew;
                    }
                }
            }
        }
    }
}

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 */
void NVCalcStateExt (
    NVPtr pNv,
    RIVA_HW_STATE *state,
    int            bpp,
    int            width,
    int            hDisplaySize,
    int            height,
    int            dotClock,
    int		   flags 
)
{
    int pixelDepth, VClk = 0;
	CARD32 CursorStart;

    /*
     * Save mode parameters.
     */
    state->bpp    = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */
    state->width  = width;
    state->height = height;
    /*
     * Extended RIVA registers.
     */
    pixelDepth = (bpp + 1)/8;
    if(pNv->twoStagePLL)
        CalcVClock2Stage(dotClock, &VClk, &state->pll, &state->pllB, pNv);
    else
        CalcVClock(dotClock, &VClk, &state->pll, pNv);

    switch (pNv->Architecture)
    {
        case NV_ARCH_04:
            nv4UpdateArbitrationSettings(VClk, 
                                         pixelDepth * 8, 
                                        &(state->arbitration0),
                                        &(state->arbitration1),
                                         pNv);
            state->cursor0  = 0x00;
            state->cursor1  = 0xbC;
	    if (flags & V_DBLSCAN)
		state->cursor1 |= 2;
            state->cursor2  = 0x00000000;
            state->pllsel   = 0x10000700;
            state->config   = 0x00001114;
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
        case NV_ARCH_10:
        case NV_ARCH_20:
        case NV_ARCH_30:
        default:
            if(((pNv->Chipset & 0xfff0) == CHIPSET_C51) ||
               ((pNv->Chipset & 0xfff0) == CHIPSET_C512))
            {
                state->arbitration0 = 128; 
                state->arbitration1 = 0x0480; 
            } else
            if(((pNv->Chipset & 0xffff) == CHIPSET_NFORCE) ||
               ((pNv->Chipset & 0xffff) == CHIPSET_NFORCE2))
            {
                nForceUpdateArbitrationSettings(VClk,
                                          pixelDepth * 8,
                                         &(state->arbitration0),
                                         &(state->arbitration1),
                                          pNv);
            } else if(pNv->Architecture < NV_ARCH_30) {
                nv10UpdateArbitrationSettings(VClk, 
                                          pixelDepth * 8, 
                                         &(state->arbitration0),
                                         &(state->arbitration1),
                                          pNv);
            } else {
                nv30UpdateArbitrationSettings(pNv,
                                         &(state->arbitration0),
                                         &(state->arbitration1));
            }
            CursorStart = pNv->Cursor->offset;
            state->cursor0  = 0x80 | (CursorStart >> 17);
            state->cursor1  = (CursorStart >> 11) << 2;
	    state->cursor2  = CursorStart >> 24;
	    if (flags & V_DBLSCAN) 
		state->cursor1 |= 2;
            state->pllsel   = 0x10000700;
            state->config   = nvReadFB(pNv, NV_PFB_CFG0);
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
    }

    if(bpp != 8) /* DirectColor */
	state->general |= 0x00000030;

    state->repaint0 = (((width / 8) * pixelDepth) & 0x700) >> 3;
    state->pixel    = (pixelDepth > 2) ? 3 : pixelDepth;
}


void NVLoadStateExt (
    ScrnInfoPtr pScrn,
    RIVA_HW_STATE *state
)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 temp;

    if(pNv->Architecture >= NV_ARCH_40) {
        switch(pNv->Chipset & 0xfff0) {
        case CHIPSET_NV44:
        case CHIPSET_NV44A:
        case CHIPSET_C51:
        case CHIPSET_G70:
        case CHIPSET_G71:
        case CHIPSET_G72:
        case CHIPSET_G73:
        case CHIPSET_C512:
             temp = nvReadCurRAMDAC(pNv, NV_RAMDAC_TEST_CONTROL);
             nvWriteCurRAMDAC(pNv, NV_RAMDAC_TEST_CONTROL, temp | 0x00100000);
             break;
        default:
             break;
        };
    }

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           nvWriteCRTC(pNv, 0, NV_CRTC_FSEL, state->head);
           nvWriteCRTC(pNv, 1, NV_CRTC_FSEL, state->head2);
        }
        temp = nvReadCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC);
        nvWriteCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC, temp | (1 << 25));
    
        nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
	nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, 0x1588, 0);

        nvWriteCurCRTC(pNv, NV_CRTC_CURSOR_CONFIG, state->cursorConfig);
        nvWriteCurCRTC(pNv, NV_CRTC_0830, state->displayV - 3);
        nvWriteCurCRTC(pNv, NV_CRTC_0834, state->displayV - 1);
    
        if(pNv->FlatPanel) {
           if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11, state->dither);
           } else 
           if(pNv->twoHeads) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER, state->dither);
           }
    
	   nvWriteVGA(pNv, NV_VGA_CRTCX_FP_HTIMING, state->timingH);
	   nvWriteVGA(pNv, NV_VGA_CRTCX_FP_VTIMING, state->timingV);
	   nvWriteVGA(pNv, NV_VGA_CRTCX_BUFFER, 0xfa);
        }

	nvWriteVGA(pNv, NV_VGA_CRTCX_EXTRA, state->extra);
    }

    nvWriteVGA(pNv, NV_VGA_CRTCX_REPAINT0, state->repaint0);
    nvWriteVGA(pNv, NV_VGA_CRTCX_REPAINT1, state->repaint1);
    nvWriteVGA(pNv, NV_VGA_CRTCX_LSR, state->screen);
    nvWriteVGA(pNv, NV_VGA_CRTCX_PIXEL, state->pixel);
    nvWriteVGA(pNv, NV_VGA_CRTCX_HEB, state->horiz);
    nvWriteVGA(pNv, NV_VGA_CRTCX_FIFO1, state->fifo);
    nvWriteVGA(pNv, NV_VGA_CRTCX_FIFO0, state->arbitration0);
    nvWriteVGA(pNv, NV_VGA_CRTCX_FIFO_LWM, state->arbitration1);
    if(pNv->Architecture >= NV_ARCH_30) {
      nvWriteVGA(pNv, NV_VGA_CRTCX_FIFO_LWM_NV30, state->arbitration1 >> 8);
    }

    nvWriteVGA(pNv, NV_VGA_CRTCX_CURCTL0, state->cursor0);
    nvWriteVGA(pNv, NV_VGA_CRTCX_CURCTL1, state->cursor1);
    if(pNv->Architecture == NV_ARCH_40) {  /* HW bug */
       volatile CARD32 curpos = nvReadCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS);
       nvWriteCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS, curpos);
    }
    nvWriteVGA(pNv, NV_VGA_CRTCX_CURCTL2, state->cursor2);
    nvWriteVGA(pNv, NV_VGA_CRTCX_INTERLACE, state->interlace);

    if(!pNv->FlatPanel) {
       nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel);
       nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL, state->vpll);
       if(pNv->twoHeads)
          nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2, state->vpll2);
       if(pNv->twoStagePLL) {
          nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL_B, state->vpllB);
          nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2_B, state->vpll2B);
       }
    } else {
       nvWriteCurRAMDAC(pNv, NV_RAMDAC_FP_CONTROL, state->scale);
       nvWriteCurRAMDAC(pNv, NV_RAMDAC_FP_HCRTC, state->crtcSync);
    }
    nvWriteCurRAMDAC(pNv, NV_RAMDAC_GENERAL_CONTROL, state->general);

    nvWriteCurCRTC(pNv, NV_CRTC_INTR_EN_0, 0);
    nvWriteCurCRTC(pNv, NV_CRTC_INTR_0, NV_CRTC_INTR_VBLANK);

    pNv->CurrentState = state;
}

void NVUnloadStateExt
(
    NVPtr pNv,
    RIVA_HW_STATE *state
)
{
    state->repaint0     = nvReadVGA(pNv, NV_VGA_CRTCX_REPAINT0);
    state->repaint1     = nvReadVGA(pNv, NV_VGA_CRTCX_REPAINT1);
    state->screen       = nvReadVGA(pNv, NV_VGA_CRTCX_LSR);
    state->pixel        = nvReadVGA(pNv, NV_VGA_CRTCX_PIXEL);
    state->horiz        = nvReadVGA(pNv, NV_VGA_CRTCX_HEB);
    state->fifo         = nvReadVGA(pNv, NV_VGA_CRTCX_FIFO1);
    state->arbitration0 = nvReadVGA(pNv, NV_VGA_CRTCX_FIFO0);
    state->arbitration1 = nvReadVGA(pNv, NV_VGA_CRTCX_FIFO_LWM);
    if(pNv->Architecture >= NV_ARCH_30) {
       state->arbitration1 |= (nvReadVGA(pNv, NV_VGA_CRTCX_FIFO_LWM_NV30) & 1) << 8;
    }
    state->cursor0      = nvReadVGA(pNv, NV_VGA_CRTCX_CURCTL0);
    state->cursor1      = nvReadVGA(pNv, NV_VGA_CRTCX_CURCTL1);
    state->cursor2      = nvReadVGA(pNv, NV_VGA_CRTCX_CURCTL2);
    state->interlace    = nvReadVGA(pNv, NV_VGA_CRTCX_INTERLACE);

    state->vpll         = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL);
    if(pNv->twoHeads)
       state->vpll2     = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2);
    if(pNv->twoStagePLL) {
        state->vpllB    = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL_B);
        state->vpll2B   = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2_B);
    }
    state->pllsel       = nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT);
    state->general      = nvReadCurRAMDAC(pNv, NV_RAMDAC_GENERAL_CONTROL);
    state->scale        = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_CONTROL);
    state->config       = nvReadFB(pNv, NV_PFB_CFG0);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           state->head     = nvReadCRTC(pNv, 0, NV_CRTC_FSEL);
           state->head2    = nvReadCRTC(pNv, 1, NV_CRTC_FSEL);
           state->crtcOwner = nvReadVGA(pNv, NV_VGA_CRTCX_OWNER);
        }
        state->extra = nvReadVGA(pNv, NV_VGA_CRTCX_EXTRA);

        state->cursorConfig = nvReadCurCRTC(pNv, NV_CRTC_CURSOR_CONFIG);

        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11);
        } else 
        if(pNv->twoHeads) {
            state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER);
        }

        if(pNv->FlatPanel) {
           state->timingH = nvReadVGA(pNv, NV_VGA_CRTCX_FP_HTIMING);
           state->timingV = nvReadVGA(pNv, NV_VGA_CRTCX_FP_VTIMING);
        }
    }

    if(pNv->FlatPanel) {
       state->crtcSync = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_HCRTC);
    }
}

void NVSetStartAddress (
    NVPtr   pNv,
    CARD32 start
)
{
    nvWriteCurCRTC(pNv, NV_CRTC_START, start);
}


