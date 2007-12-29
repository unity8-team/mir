#ifndef __NV_SHADERS_H__
#define __NV_SHADERS_H__

#define NV_SHADER_MAX_PROGRAM_LENGTH 256

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

static void
NV40_LoadVtxProg(ScrnInfoPtr pScrn, nv_shader_t *shader)
{
	NVPtr pNv = NVPTR(pScrn);
	static int next_hw_id = 0;
	int i;

	if (!shader->hw_id) {
		shader->hw_id = next_hw_id;

		BEGIN_RING(Nv3D, NV40TCL_VP_UPLOAD_FROM_ID, 1);
		OUT_RING  ((shader->hw_id));
		for (i=0; i<shader->size; i+=4) {
			BEGIN_RING(Nv3D, NV40TCL_VP_UPLOAD_INST(0), 4);
			OUT_RING  (shader->data[i + 0]);
			OUT_RING  (shader->data[i + 1]);
			OUT_RING  (shader->data[i + 2]);
			OUT_RING  (shader->data[i + 3]);
			next_hw_id++;
		}
	}

	BEGIN_RING(Nv3D, NV40TCL_VP_START_FROM_ID, 1);
	OUT_RING  ((shader->hw_id));

	BEGIN_RING(Nv3D, NV40TCL_VP_ATTRIB_EN, 2);
	OUT_RING  (shader->card_priv.NV30VP.vp_in_reg);
	OUT_RING  (shader->card_priv.NV30VP.vp_out_reg);
}

static void
NV40_LoadFragProg(ScrnInfoPtr pScrn, nv_shader_t *shader)
{
	NVPtr pNv = NVPTR(pScrn);
	static struct nouveau_bo *fp_mem = NULL;
	static int next_hw_id_offset = 0;

	if (!fp_mem) {
		if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART,
				0, 0x1000, &fp_mem)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Couldn't alloc fragprog buffer!\n");
			return;
		}

		if (nouveau_bo_map(fp_mem, NOUVEAU_BO_RDWR)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Couldn't map fragprog buffer!\n");
		}
	}

	if (!shader->hw_id) {
		uint32_t *map = fp_mem->map + next_hw_id_offset;
		int i;

		for (i = 0; i < shader->size; i++) {
			uint32_t data = shader->data[i];
#if (X_BYTE_ORDER != X_LITTLE_ENDIAN)
			data = ((data >> 16) | ((data & 0xffff) << 16));
#endif
			map[i] = data;
		}

		shader->hw_id = next_hw_id_offset;
		next_hw_id_offset += (shader->size * sizeof(uint32_t));
		next_hw_id_offset = (next_hw_id_offset + 63) & ~63;
	}

	BEGIN_RING(Nv3D, NV40TCL_FP_ADDRESS, 1);
	OUT_RELOC (fp_mem, shader->hw_id, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART |
		   NOUVEAU_BO_RD | NOUVEAU_BO_LOW | NOUVEAU_BO_OR,
		   NV40TCL_FP_ADDRESS_DMA0, NV40TCL_FP_ADDRESS_DMA1);
	BEGIN_RING(Nv3D, NV40TCL_FP_CONTROL, 1);
	OUT_RING  (shader->card_priv.NV30FP.num_regs <<
		   NV40TCL_FP_CONTROL_TEMP_COUNT_SHIFT);
}

/*******************************************************************************
 * NV40/G70 vertex shaders
 */

static nv_shader_t nv40_vp_exa_render = {
  .card_priv.NV30VP.vp_in_reg  = 0x00000309,
  .card_priv.NV30VP.vp_out_reg = 0x0000c001,
  .size = (3*4),
  .data = {
    /* MOV result.position, vertex.position */
    0x40041c6c, 0x0040000d, 0x8106c083, 0x6041ff80,
    /* MOV result.texcoord[0], vertex.texcoord[0] */
    0x401f9c6c, 0x0040080d, 0x8106c083, 0x6041ff9c,
    /* MOV result.texcoord[1], vertex.texcoord[1] */
    0x401f9c6c, 0x0040090d, 0x8106c083, 0x6041ffa1,
  }
};

/*******************************************************************************
 * NV30/NV40/G70 fragment shaders
 */

static nv_shader_t nv30_fp_pass_col0 = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (1*4),
  .data = {
    /* MOV R0, fragment.color */
    0x01403e81, 0x1c9dc801, 0x0001c800, 0x3fe1c800, 
  }
};

static nv_shader_t nv30_fp_pass_tex0 = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (2*4),
  .data = {
    /* TEX R0, fragment.texcoord[0], texture[0], 2D */
    0x17009e00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* MOV R0, R0 */
    0x01401e81, 0x1c9dc800, 0x0001c800, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R1.w         , fragment.texcoord[1], texture[1], 2D */
    0x1702b102, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R0 (NE0.wwww), fragment.texcoord[0], texture[0], 2D */
    0x17009e00, 0x1ff5c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R0, R1.w */
    0x02001e81, 0x1c9dc800, 0x0001fe04, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask_sa_ca = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R1.w         , fragment.texcoord[0], texture[0], 2D */
    0x17009102, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R0 (NE0.wwww), fragment.texcoord[1], texture[1], 2D */
    0x1702be00, 0x1ff5c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R1,wwww, R0 */
    0x02001e81, 0x1c9dfe04, 0x0001c800, 0x0001c800,
  }
};

static nv_shader_t nv30_fp_composite_mask_ca = {
  .card_priv.NV30FP.num_regs = 2,
  .size = (3*4),
  .data = {
    /* TEXC0 R0           , fragment.texcoord[0], texture[0], 2D */
    0x17009f00, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
    /* TEX   R1 (NE0.xyzw), fragment.texcoord[1], texture[1], 2D */
    0x1702be02, 0x1c95c801, 0x0001c800, 0x3fe1c800,
    /* MUL   R0           , R0, R1 */
    0x02001e81, 0x1c9dc800, 0x0001c804, 0x0001c800,
  }
};

#endif
