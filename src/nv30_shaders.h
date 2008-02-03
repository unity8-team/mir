#ifndef __NV30_SHADERS_H__
#define __NV30_SHADERS_H__

#define NV_SHADER_MAX_PROGRAM_LENGTH 256

#include "nv_include.h"

typedef struct nv_shader {
	uint32_t hw_id;
	uint32_t size;
	union {
		struct {
			uint32_t vp_in_reg;
			uint32_t vp_out_reg;
		} NV30VP;
		struct  {
			uint32_t num_regs;
		} NV30FP;
	} card_priv;
	uint32_t data[NV_SHADER_MAX_PROGRAM_LENGTH];
} nv_shader_t;

void NV40_LoadVtxProg(ScrnInfoPtr pScrn, nv_shader_t *shader);
void NV40_LoadFragProg(ScrnInfoPtr pScrn, nv_shader_t *shader);
void NV30_LoadFragProg(ScrnInfoPtr pScrn, nv_shader_t *shader);


/*******************************************************************************
 * NV40/G70 vertex shaders
 */

nv_shader_t nv40_vp_exa_render;
nv_shader_t nv40_vp_video;

/*******************************************************************************
 * NV30/NV40/G70 fragment shaders
 */

nv_shader_t nv30_fp_pass_col0;
nv_shader_t nv30_fp_pass_tex0;
nv_shader_t nv30_fp_composite_mask;
nv_shader_t nv30_fp_composite_mask_sa_ca;
nv_shader_t nv30_fp_composite_mask_ca;
nv_shader_t nv30_fp_yv12_bicubic;
nv_shader_t nv30_fp_yv12_bilinear;
nv_shader_t nv40_fp_yv12_bicubic;

#endif
