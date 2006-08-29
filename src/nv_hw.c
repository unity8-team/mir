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

#include "nv_local.h"
#include "compiler.h"
#include "nv_include.h"

uint8_t nvReadVGA(NVPtr pNv, uint8_t index)
{
  volatile const uint8_t *ptr = pNv->cur_head ? pNv->PCIO1 : pNv->PCIO0;
  VGA_WR08(ptr, 0x03D4, index);
  return VGA_RD08(ptr, 0x03D5);
}

void nvWriteVGA(NVPtr pNv, uint8_t index, uint8_t data)
{
  volatile const uint8_t *ptr = pNv->cur_head ? pNv->PCIO1 : pNv->PCIO0;
  VGA_WR08(ptr, 0x03D4, index);
  VGA_WR08(ptr, 0x03D5, data);
}

CARD32 nvReadRAMDAC(NVPtr pNv, uint8_t head, uint32_t ramdac_reg)
{
  volatile const void *ptr = head ? pNv->PRAMDAC1 : pNv->PRAMDAC0;
  return MMIO_IN32(ptr, ramdac_reg);
}

void nvWriteRAMDAC(NVPtr pNv, uint8_t head, uint32_t ramdac_reg, CARD32 val)
{
  volatile const void *ptr = head ? pNv->PRAMDAC1 : pNv->PRAMDAC0;
  MMIO_OUT32(ptr, ramdac_reg, val);
}


void NVLockUnlock (
    NVPtr pNv,
    Bool  Lock
)
{
    CARD8 cr11;

    nvWriteVGA(pNv, 0x1f, Lock ? 0x99 : 0x57 );

    cr11 = nvReadVGA(pNv, 0x11);
    if(Lock) cr11 |= 0x80;
    else cr11 &= ~0x80;
    nvWriteVGA(pNv, 0x11, cr11);
}

int NVShowHideCursor (
    NVPtr pNv,
    int   ShowHide
)
{
    int current = pNv->CurrentState->cursor1;

    pNv->CurrentState->cursor1 = (pNv->CurrentState->cursor1 & 0xFE) |
                                 (ShowHide & 0x01);

    nvWriteVGA(pNv, 0x31, pNv->CurrentState->cursor1);

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
  int pclk_khz;
  int mclk_khz;
  int nvclk_khz;
  char mem_page_miss;
  char mem_latency;
  int memory_type;
  int memory_width;
  char enable_video;
  char gr_during_vid;
  char pix_bpp;
  char mem_aligned;
  char enable_mp;
} nv10_sim_state;


static void nvGetClocks(NVPtr pNv, unsigned int *MClk, unsigned int *NVClk)
{
    unsigned int pll, N, M, MB, NB, P;

    if(pNv->Architecture >= NV_ARCH_40) {
       pll = nvReadMC(pNv, 0x4020);
       P = (pll >> 16) & 0x07;
       pll = nvReadMC(pNv, 0x4024);
       M = pll & 0xFF;
       N = (pll >> 8) & 0xFF;
       if(((pNv->Chipset & 0xfff0) == CHIPSET_G71) ||
          ((pNv->Chipset & 0xfff0) == CHIPSET_G73))
       {
          MB = 1;
          NB = 1;
       } else {
          MB = (pll >> 16) & 0xFF;
          NB = (pll >> 24) & 0xFF;
       }
       *MClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;

       pll = nvReadMC(pNv, 0x4000);
       P = (pll >> 16) & 0x07;  
       pll = nvReadMC(pNv, 0x4004);
       M = pll & 0xFF;
       N = (pll >> 8) & 0xFF;
       MB = (pll >> 16) & 0xFF;
       NB = (pll >> 24) & 0xFF;

       *NVClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;
    } else
    if(pNv->twoStagePLL) {
       pll = nvReadRAMDAC0(pNv, 0x0504);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       pll = nvReadRAMDAC0(pNv, 0x0574);
       if(pll & 0x80000000) {
           MB = pll & 0xFF; 
           NB = (pll >> 8) & 0xFF;
       } else {
           MB = 1;
           NB = 1;
       }
       *MClk = ((N * NB * pNv->CrystalFreqKHz) / (M * MB)) >> P;

       pll = nvReadRAMDAC0(pNv, 0x0500);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       pll = nvReadRAMDAC0(pNv, 0x0570);
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
       pll = nvReadRAMDAC0(pNv, 0x504);
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

       pll = nvReadRAMDAC0(pNv, 0x500);
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
       pll = nvReadRAMDAC0(pNv, 0x504);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       *MClk = (N * pNv->CrystalFreqKHz / M) >> P;

       pll = nvReadRAMDAC0(pNv, 0x500);
       M = pll & 0xFF; 
       N = (pll >> 8) & 0xFF; 
       P = (pll >> 16) & 0x0F;
       *NVClk = (N * pNv->CrystalFreqKHz / M) >> P;
    }

#if 0
    ErrorF("NVClock = %i MHz, MEMClock = %i MHz\n", *NVClk/1000, *MClk/1000);
#endif
}


static void nv4CalcArbitration (
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

static void nv4UpdateArbitrationSettings (
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

    cfg1 = nvReadFB(pNv, 0x204);
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

static void nv10CalcArbitration (
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

static void nv10UpdateArbitrationSettings (
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

    cfg1 = nvReadFB(pNv, 0x0204);
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 1;
    sim_data.enable_mp      = 0;
    sim_data.memory_type    = (nvReadFB(pNv, 0x0200) & 0x01) ? 1 : 0;
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


static void nv30UpdateArbitrationSettings (
    NVPtr        pNv,
    unsigned     *burst,
    unsigned     *lwm
)   
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

static void nForceUpdateArbitrationSettings (
    unsigned      VClk,
    unsigned      pixelDepth,
    unsigned     *burst,
    unsigned     *lwm,
    NVPtr        pNv
)
{
    nv10_fifo_info fifo_data;
    nv10_sim_state sim_data;
    unsigned int M, N, P, pll, MClk, NVClk, memctrl;

    if((pNv->Chipset & 0x0FF0) == CHIPSET_NFORCE) {
       unsigned int uMClkPostDiv;

       uMClkPostDiv = (pciReadLong(pciTag(0, 0, 3), 0x6C) >> 8) & 0xf;
       if(!uMClkPostDiv) uMClkPostDiv = 4; 
       MClk = 400000 / uMClkPostDiv;
    } else {
       MClk = pciReadLong(pciTag(0, 0, 5), 0x4C) / 1000;
    }

    pll = nvReadRAMDAC0(pNv, 0x500);
    M = (pll >> 0)  & 0xFF; N = (pll >> 8)  & 0xFF; P = (pll >> 16) & 0x0F;
    NVClk  = (N * pNv->CrystalFreqKHz / M) >> P;
    sim_data.pix_bpp        = (char)pixelDepth;
    sim_data.enable_video   = 0;
    sim_data.enable_mp      = 0;
    sim_data.memory_type    = (pciReadLong(pciTag(0, 0, 1), 0x7C) >> 12) & 1;
    sim_data.memory_width   = 64;

    memctrl = pciReadLong(pciTag(0, 0, 3), 0x00) >> 16;

    if((memctrl == 0x1A9) || (memctrl == 0x1AB) || (memctrl == 0x1ED)) {
        int dimm[3];

        dimm[0] = (pciReadLong(pciTag(0, 0, 2), 0x40) >> 8) & 0x4F;
        dimm[1] = (pciReadLong(pciTag(0, 0, 2), 0x44) >> 8) & 0x4F;
        dimm[2] = (pciReadLong(pciTag(0, 0, 2), 0x48) >> 8) & 0x4F;

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
    int pixelDepth, VClk;
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
			CursorStart = pNv->Cursor->offset - pNv->VRAMPhysical;
            state->cursor0  = 0x80 | (CursorStart >> 17);
            state->cursor1  = (CursorStart >> 11) << 2;
	    state->cursor2  = CursorStart >> 24;
	    if (flags & V_DBLSCAN) 
		state->cursor1 |= 2;
            state->pllsel   = 0x10000700;
            state->config   = nvReadFB(pNv, 0x200);
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
    int i, j;
    CARD32 temp;

    if (!pNv->IRQ)
        nvWriteMC(pNv, 0x140, 0);
    nvWriteMC(pNv, 0x0200, 0xFFFF00FF);
    nvWriteMC(pNv, 0x0200, 0xFFFFFFFF);

    nvWriteTIMER(pNv, 0x0200, 0x00000008);
    nvWriteTIMER(pNv, 0x0210, 0x00000003);
    /*TODO: DRM handle PTIMER interrupts */
    nvWriteTIMER(pNv, 0x0140, 0x00000000);
    nvWriteTIMER(pNv, 0x0100, 0xFFFFFFFF);

    /* begin surfaces */
    /* it seems those regions are equivalent to the radeon's SURFACEs. needs to go in-kernel just like the SURFACEs */
    if(pNv->Architecture == NV_ARCH_04) {
        nvWriteFB(pNv, 0x0200, state->config);
    } else 
    if((pNv->Architecture < NV_ARCH_40) ||
       ((pNv->Chipset & 0xfff0) == CHIPSET_NV40))
    {
        for(i = 0; i < 8; i++) {
           nvWriteFB(pNv, (0x0240 + (i * 0x10)), 0);
           nvWriteFB(pNv, (0x0244 + (i * 0x10)), pNv->VRAMPhysicalSize - 1);
        }
    } else {
        int regions = 12;

        if(((pNv->Chipset & 0xfff0) == CHIPSET_G70) ||
           ((pNv->Chipset & 0xfff0) == CHIPSET_G71) ||
           ((pNv->Chipset & 0xfff0) == CHIPSET_G72) ||
           ((pNv->Chipset & 0xfff0) == CHIPSET_G73) ||
           ((pNv->Chipset & 0xfff0) == CHIPSET_C512))
        {
           regions = 15;
        }
 
       for(i = 0; i < regions; i++) {
          nvWriteFB(pNv, (0x0600 + (i * 0x10)), 0);
          nvWriteFB(pNv, (0x0604 + (i * 0x10)), pNv->VRAMPhysicalSize - 1);
       }
    }
    /* end of surfaces */

    if(pNv->Architecture < NV_ARCH_10) {
       if((pNv->Chipset & 0x0fff) == CHIPSET_NV04) {
		   /*XXX: RAMIN access here, find out what it's for.
			*     The DRM is handling RAMIN now
			*/
           pNv->PRAMIN[0x0824] |= 0x00020000;
           pNv->PRAMIN[0x0826] += pNv->VRAMPhysical;
       }
       nvWriteGRAPH(pNv, 0x0080, 0x000001FF);
       nvWriteGRAPH(pNv, 0x0080, 0x1230C000);
       nvWriteGRAPH(pNv, 0x0084, 0x72111101);
       nvWriteGRAPH(pNv, 0x0088, 0x11D5F071);
       nvWriteGRAPH(pNv, 0x008C, 0x0004FF31);
       nvWriteGRAPH(pNv, 0x008C, 0x4004FF31);

       if (!pNv->IRQ) {
	   nvWriteGRAPH(pNv, 0x140, 0x0);
	   nvWriteGRAPH(pNv, 0x100, 0xFFFFFFFF);
       }
       nvWriteGRAPH(pNv, 0x0170, 0x10010100);
       nvWriteGRAPH(pNv, 0x0710, 0xFFFFFFFF);
       nvWriteGRAPH(pNv, 0x0720, 0x00000001);

       nvWriteGRAPH(pNv, 0x0810, 0x00000000);
       nvWriteGRAPH(pNv, 0x0608, 0xFFFFFFFF);
    } else {
       nvWriteGRAPH(pNv, 0x0080, 0xFFFFFFFF);
       nvWriteGRAPH(pNv, 0x0080, 0x00000000);

       if (!pNv->IRQ) {
	   nvWriteGRAPH(pNv, 0x140, 0x0);
	   nvWriteGRAPH(pNv, 0x100, 0xFFFFFFFF);
       }
       nvWriteGRAPH(pNv, 0x0144, 0x10010100);
       nvWriteGRAPH(pNv, 0x0714, 0xFFFFFFFF);
       nvWriteGRAPH(pNv, 0x0720, 0x00000001);
       temp = nvReadGRAPH(pNv, 0x710);
       nvWriteGRAPH(pNv, 0x0710, temp & 0x0007ff00);
       temp = nvReadGRAPH(pNv, 0x710);
       nvWriteGRAPH(pNv, 0x0710, temp | 0x00020100);

       if(pNv->Architecture == NV_ARCH_10) {
           nvWriteGRAPH(pNv, 0x0084, 0x00118700);
           nvWriteGRAPH(pNv, 0x0088, 0x24E00810);
           nvWriteGRAPH(pNv, 0x008C, 0x55DE0030);

           /* nv10 second surfaces */
           /* this is a copy of the surfaces. What is it for ? */
           for(i = 0; i < 32; i++)
             nvWriteGRAPH(pNv, 0x0B00 + (i*4), nvReadFB(pNv, 0x0240 + (i*4)));
           /* end of nv10 second surfaces */

           nvWriteGRAPH(pNv, 0x640, 0);
           nvWriteGRAPH(pNv, 0x644, 0);
	   nvWriteGRAPH(pNv, 0x684, pNv->VRAMPhysicalSize - 1);
           nvWriteGRAPH(pNv, 0x688, pNv->VRAMPhysicalSize - 1);

           nvWriteGRAPH(pNv, 0x0810, 0x00000000);
           nvWriteGRAPH(pNv, 0x0608, 0xFFFFFFFF);
       } else {
           if(pNv->Architecture >= NV_ARCH_40) {
              nvWriteGRAPH(pNv, 0x0084, 0x401287c0);
              nvWriteGRAPH(pNv, 0x008C, 0x60de8051);
              nvWriteGRAPH(pNv, 0x0090, 0x00008000);
              nvWriteGRAPH(pNv, 0x0610, 0x00be3c5f);

              j = pNv->REGS[0x1540/4] & 0xff;
              if(j) {
                  for(i = 0; !(j & 1); j >>= 1, i++);
                  nvWriteGRAPH(pNv, 0x5000, i);
              }

              if((pNv->Chipset & 0xfff0) == CHIPSET_NV40) {
                 nvWriteGRAPH(pNv, 0x09b0, 0x83280fff);
                 nvWriteGRAPH(pNv, 0x09b4, 0x000000a0);
              } else {
                 nvWriteGRAPH(pNv, 0x0820, 0x83280eff);
                 nvWriteGRAPH(pNv, 0x0824, 0x000000a0);
              }

              switch(pNv->Chipset & 0xfff0) {
              case CHIPSET_NV40:
              case CHIPSET_NV45:
                 nvWriteGRAPH(pNv, 0x09b8, 0x0078e366);
                 nvWriteGRAPH(pNv, 0x09bc, 0x0000014c);
		 temp = nvReadFB(pNv, 0x33C);
		 nvWriteFB(pNv, 0x33c, temp & 0xffff7fff);
                 break;
              case CHIPSET_NV41:
              case 0x0120:
                 nvWriteGRAPH(pNv, 0x0828, 0x007596ff);
                 nvWriteGRAPH(pNv, 0x082C, 0x00000108);
                 break;
              case CHIPSET_NV44:
              case CHIPSET_G72:
              case CHIPSET_C51:
              case CHIPSET_C512:
                 nvWriteMC(pNv, 0x1700, nvReadFB(pNv, 0x020C));
                 nvWriteMC(pNv, 0x1704, 0);
                 nvWriteMC(pNv, 0x1708, 0);
                 nvWriteMC(pNv, 0x170C, nvReadFB(pNv, 0x020C));
                 nvWriteGRAPH(pNv, 0x0860, 0);
                 nvWriteGRAPH(pNv, 0x0864, 0);
                 temp = nvReadCurRAMDAC(pNv, 0x608);
                 nvWriteCurRAMDAC(pNv, 0x608, temp | 0x00100000);
                 break;
              case CHIPSET_NV43:
                 nvWriteGRAPH(pNv, 0x0828, 0x0072cb77);
                 nvWriteGRAPH(pNv, 0x082C, 0x00000108);
                 break;
              case CHIPSET_NV44A:
                 nvWriteGRAPH(pNv, 0x0860, 0);
                 nvWriteGRAPH(pNv, 0x0864, 0);
                 temp = nvReadCurRAMDAC(pNv, 0x608);
                 nvWriteCurRAMDAC(pNv, 0x608, temp | 0x00100000);
                 break;
              case CHIPSET_G70:
              case CHIPSET_G71:
              case CHIPSET_G73:
                 temp = nvReadCurRAMDAC(pNv, 0x608);
                 nvWriteCurRAMDAC(pNv, 0x608, temp | 0x00100000);
                 nvWriteGRAPH(pNv, 0x0828, 0x07830610);
                 nvWriteGRAPH(pNv, 0x082C, 0x0000016A);
                 break;
              default:
                 break;
              };

              nvWriteGRAPH(pNv, 0x0b38, 0x2ffff800);
              nvWriteGRAPH(pNv, 0x0b3c, 0x00006000);
              nvWriteGRAPH(pNv, 0x032C, 0x01000000); 
           } else
           if(pNv->Architecture == NV_ARCH_30) {
              nvWriteGRAPH(pNv, 0x0084, 0x40108700);
              nvWriteGRAPH(pNv, 0x0890, 0x00140000);
              nvWriteGRAPH(pNv, 0x008C, 0xf00e0431);
              nvWriteGRAPH(pNv, 0x0090, 0x00008000);
              nvWriteGRAPH(pNv, 0x0610, 0xf04b1f36);
              nvWriteGRAPH(pNv, 0x0B80, 0x1002d888);
              nvWriteGRAPH(pNv, 0x0B88, 0x62ff007f);
           } else {
              nvWriteGRAPH(pNv, 0x0084, 0x00118700);
              nvWriteGRAPH(pNv, 0x008C, 0xF20E0431);
              nvWriteGRAPH(pNv, 0x0090, 0x00000000);
              nvWriteGRAPH(pNv, 0x009C, 0x00000040);

              if((pNv->Chipset & 0x0ff0) >= CHIPSET_NV25) {
                 nvWriteGRAPH(pNv, 0x0890, 0x00080000);
                 nvWriteGRAPH(pNv, 0x0610, 0x304B1FB6); 
                 nvWriteGRAPH(pNv, 0x0B80, 0x18B82880); 
                 nvWriteGRAPH(pNv, 0x0B84, 0x44000000); 
                 nvWriteGRAPH(pNv, 0x0098, 0x40000080); 
                 nvWriteGRAPH(pNv, 0x0B88, 0x000000ff); 
              } else {
                 nvWriteGRAPH(pNv, 0x0880, 0x00080000);
                 nvWriteGRAPH(pNv, 0x0094, 0x00000005);
                 nvWriteGRAPH(pNv, 0x0B80, 0x45CAA208); 
                 nvWriteGRAPH(pNv, 0x0B84, 0x24000000);
                 nvWriteGRAPH(pNv, 0x0098, 0x00000040);
                 nvWriteGRAPH(pNv, 0x0750, 0x00E00038);
                 nvWriteGRAPH(pNv, 0x0754, 0x00000030);
                 nvWriteGRAPH(pNv, 0x0750, 0x00E10038);
                 nvWriteGRAPH(pNv, 0x0754, 0x00000030);
              }
           }

           /* begin nv20+ secondr surfaces */
           /* again, a copy of the surfaces. */
           if((pNv->Architecture < NV_ARCH_40) ||
              ((pNv->Chipset & 0xfff0) == CHIPSET_NV40)) 
           {
              for(i = 0; i < 32; i++) {
                nvWriteGRAPH(pNv, 0x0900 + i*4, nvReadFB(pNv, 0x0240 + i*4));
                nvWriteGRAPH(pNv, 0x6900 + i*4, nvReadFB(pNv, 0x0240 + i*4));
              }
           } else {
              if(((pNv->Chipset & 0xfff0) == CHIPSET_G70) ||
                 ((pNv->Chipset & 0xfff0) == CHIPSET_G71) ||
                 ((pNv->Chipset & 0xfff0) == CHIPSET_G72) ||
                 ((pNv->Chipset & 0xfff0) == CHIPSET_G73) ||
                 ((pNv->Chipset & 0xfff0) == CHIPSET_C512))
              {
                 for(i = 0; i < 60; i++) {
                   nvWriteGRAPH(pNv, 0x0D00 + i*4, nvReadFB(pNv, 0x0600 + i*4));
                   nvWriteGRAPH(pNv, 0x6900 + i*4, nvReadFB(pNv, 0x0600 + i*4));
                 }
              } else {
                 for(i = 0; i < 48; i++) {
                   nvWriteGRAPH(pNv, 0x0900 + i*4, nvReadFB(pNv, 0x0600 + i*4));
                   if(((pNv->Chipset & 0xfff0) != CHIPSET_NV44) &&
                      ((pNv->Chipset & 0xfff0) != CHIPSET_NV44A) &&
                      ((pNv->Chipset & 0xfff0) != CHIPSET_C51))
                   {
                      nvWriteGRAPH(pNv, 0x6900 + i*4, nvReadFB(pNv, 0x0600 + i*4));
                   }
                 }
              }
           }
           /* end nv20+ second surfaces */

           /* begin RAM config */
           if(pNv->Architecture >= NV_ARCH_40) {
              if((pNv->Chipset & 0xfff0) == CHIPSET_NV40) {
                 nvWriteGRAPH(pNv, 0x09A4, nvReadFB(pNv, 0x0200));
                 nvWriteGRAPH(pNv, 0x09A8, nvReadFB(pNv, 0x0204));

                 nvWriteGRAPH(pNv, 0x0820, 0);
                 nvWriteGRAPH(pNv, 0x0824, 0);
                 nvWriteGRAPH(pNv, 0x0864, pNv->VRAMPhysicalSize - 1);
                 nvWriteGRAPH(pNv, 0x0868, pNv->VRAMPhysicalSize - 1);
              } else {
                 if(((pNv->Chipset & 0xfff0) == CHIPSET_G70) ||
                    ((pNv->Chipset & 0xfff0) == CHIPSET_G71) ||
                    ((pNv->Chipset & 0xfff0) == CHIPSET_G72) ||
                    ((pNv->Chipset & 0xfff0) == CHIPSET_G73)) 
                 {
                    nvWriteGRAPH(pNv, 0x0DF0, nvReadFB(pNv, 0x0200));
                    nvWriteGRAPH(pNv, 0x0DF4, nvReadFB(pNv, 0x0204));
                 } else {
                    nvWriteGRAPH(pNv, 0x09F0, nvReadFB(pNv, 0x0200));
                    nvWriteGRAPH(pNv, 0x09F4, nvReadFB(pNv, 0x0204));
                 }
                 nvWriteGRAPH(pNv, 0x69F0, nvReadFB(pNv, 0x0200));
                 nvWriteGRAPH(pNv, 0x69F4, nvReadFB(pNv, 0x0204));

                 nvWriteGRAPH(pNv, 0x0840, 0);
                 nvWriteGRAPH(pNv, 0x0844, 0);
                 nvWriteGRAPH(pNv, 0x08a0, pNv->VRAMPhysicalSize - 1);
                 nvWriteGRAPH(pNv, 0x08a4, pNv->VRAMPhysicalSize - 1);
              }
           } else {
              nvWriteGRAPH(pNv, 0x09A4, nvReadFB(pNv, 0x0200));
              nvWriteGRAPH(pNv, 0x09A8, nvReadFB(pNv, 0x0204));
              nvWriteGRAPH(pNv, 0x0750, 0x00EA0000);
              nvWriteGRAPH(pNv, 0x0754, nvReadFB(pNv, 0x0200));
              nvWriteGRAPH(pNv, 0x0750, 0x00EA0004);
              nvWriteGRAPH(pNv, 0x0754, nvReadFB(pNv, 0x0204));

              nvWriteGRAPH(pNv, 0x0820, 0);
              nvWriteGRAPH(pNv, 0x0824, 0);
              nvWriteGRAPH(pNv, 0x0864, pNv->VRAMPhysicalSize - 1);
              nvWriteGRAPH(pNv, 0x0868, pNv->VRAMPhysicalSize - 1);
           }
           /* end of RAM config */

           nvWriteGRAPH(pNv, 0x0B20, 0x00000000);
           nvWriteGRAPH(pNv, 0x0B04, 0xFFFFFFFF);
       }
    }

    /* begin clipping values */
    nvWriteGRAPH(pNv, 0x053C, 0);
    nvWriteGRAPH(pNv, 0x0540, 0);
    nvWriteGRAPH(pNv, 0x0544, 0x00007FFF);
    nvWriteGRAPH(pNv, 0x0548, 0x00007FFF);
    /* end of clipping values */

    /* Seems we have to reinit some/all of the FIFO regs on a mode switch */
    drmCommandNone(pNv->drm_fd, DRM_NOUVEAU_PFIFO_REINIT);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           pNv->PCRTC0[0x0860/4] = state->head;
           pNv->PCRTC0[0x2860/4] = state->head2;
        }
        temp = nvReadCurRAMDAC(pNv, 0x404);
        nvWriteCurRAMDAC(pNv, 0x404, temp | (1 << 25));
    
        nvWriteMC(pNv, 0x8704, 1);
        nvWriteMC(pNv, 0x8140, 0);
        nvWriteMC(pNv, 0x8920, 0);
        nvWriteMC(pNv, 0x8924, 0);
        nvWriteMC(pNv, 0x8908, pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, 0x890C, pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, 0x1588, 0);

        pNv->PCRTC[0x0810/4] = state->cursorConfig;
        pNv->PCRTC[0x0830/4] = state->displayV - 3;
        pNv->PCRTC[0x0834/4] = state->displayV - 1;
    
        if(pNv->FlatPanel) {
           if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
               nvWriteCurRAMDAC(pNv, 0x528, state->dither);
           } else 
           if(pNv->twoHeads) {
               nvWriteCurRAMDAC(pNv, 0x83C, state->dither);
           }
    
	   nvWriteVGA(pNv, 0x53, state->timingH);
	   nvWriteVGA(pNv, 0x54, state->timingV);
	   nvWriteVGA(pNv, 0x21, 0xfa);
        }

	nvWriteVGA(pNv, 0x41, state->extra);
    }

    nvWriteVGA(pNv, 0x19, state->repaint0);
    nvWriteVGA(pNv, 0x1A, state->repaint1);
    nvWriteVGA(pNv, 0x25, state->screen);
    nvWriteVGA(pNv, 0x28, state->pixel);
    nvWriteVGA(pNv, 0x2D, state->horiz);
    nvWriteVGA(pNv, 0x1C, state->fifo);
    nvWriteVGA(pNv, 0x1B, state->arbitration0);
    nvWriteVGA(pNv, 0x20, state->arbitration1);
    if(pNv->Architecture >= NV_ARCH_30) {
      nvWriteVGA(pNv, 0x47, state->arbitration1 >> 8);
    }

    nvWriteVGA(pNv, 0x30, state->cursor0);
    nvWriteVGA(pNv, 0x31, state->cursor1);
    nvWriteVGA(pNv, 0x2F, state->cursor2);
    nvWriteVGA(pNv, 0x39, state->interlace);

    if(!pNv->FlatPanel) {
       nvWriteRAMDAC0(pNv, 0x50C, state->pllsel);
       nvWriteRAMDAC0(pNv, 0x508, state->vpll);
       if(pNv->twoHeads)
          nvWriteRAMDAC0(pNv, 0x520, state->vpll2);
       if(pNv->twoStagePLL) {
          nvWriteRAMDAC0(pNv, 0x578, state->vpllB);
          nvWriteRAMDAC0(pNv, 0x57C, state->vpll2B);
       }
    } else {
       nvWriteCurRAMDAC(pNv, 0x848, state->scale);
       nvWriteCurRAMDAC(pNv, 0x828, state->crtcSync);
    }
    nvWriteCurRAMDAC(pNv, 0x600, state->general);

    pNv->PCRTC[0x0140/4] = 0;
    pNv->PCRTC[0x0100/4] = 1;

    pNv->CurrentState = state;
}

void NVUnloadStateExt
(
    NVPtr pNv,
    RIVA_HW_STATE *state
)
{
    state->repaint0     = nvReadVGA(pNv, 0x19);
    state->repaint1     = nvReadVGA(pNv, 0x1A);
    state->screen       = nvReadVGA(pNv, 0x25);
    state->pixel        = nvReadVGA(pNv, 0x28);
    state->horiz        = nvReadVGA(pNv, 0x2D);
    state->fifo         = nvReadVGA(pNv, 0x1C);
    state->arbitration0 = nvReadVGA(pNv, 0x1B);
    state->arbitration1 = nvReadVGA(pNv, 0x20);
    if(pNv->Architecture >= NV_ARCH_30) {
       state->arbitration1 |= (nvReadVGA(pNv, 0x47) & 1) << 8;
    }
    state->cursor0      = nvReadVGA(pNv, 0x30);
    state->cursor1      = nvReadVGA(pNv, 0x31);
    state->cursor2      = nvReadVGA(pNv, 0x2F);
    state->interlace    = nvReadVGA(pNv, 0x39);

    state->vpll         = nvReadRAMDAC0(pNv, 0x0508);
    if(pNv->twoHeads)
       state->vpll2     = nvReadRAMDAC0(pNv, 0x0520);
    if(pNv->twoStagePLL) {
        state->vpllB    = nvReadRAMDAC0(pNv, 0x0578);
        state->vpll2B   = nvReadRAMDAC0(pNv, 0x057C);
    }
    state->pllsel       = nvReadRAMDAC0(pNv, 0x050C);
    state->general      = nvReadCurRAMDAC(pNv, 0x0600);
    state->scale        = nvReadCurRAMDAC(pNv, 0x0848);
    state->config       = nvReadFB(pNv, 0x0200);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           state->head     = pNv->PCRTC0[0x0860/4];
           state->head2    = pNv->PCRTC0[0x2860/4];
           state->crtcOwner = nvReadVGA(pNv, 0x44);
        }
        state->extra = nvReadVGA(pNv, 0x41);

        state->cursorConfig = pNv->PCRTC[0x0810/4];

        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           state->dither = nvReadCurRAMDAC(pNv, 0x0528);
        } else 
        if(pNv->twoHeads) {
            state->dither = nvReadCurRAMDAC(pNv, 0x083C);
        }

        if(pNv->FlatPanel) {
           state->timingH = nvReadVGA(pNv, 0x53);
           state->timingV = nvReadVGA(pNv, 0x54);
        }
    }

    if(pNv->FlatPanel) {
       state->crtcSync = nvReadCurRAMDAC(pNv, 0x0828);
    }
}

void NVSetStartAddress (
    NVPtr   pNv,
    CARD32 start
)
{
    pNv->PCRTC[0x800/4] = start;
}


