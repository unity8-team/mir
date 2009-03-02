#ifndef __R600_STATE_H__
#define __R600_STATE_H__


#include "xf86drm.h"

typedef int bool_t;

#define CLEAR(x) memset (&x, 0, sizeof(x))

/* Sequencer / thread handling */
typedef struct {
    int ps_prio;
    int vs_prio;
    int gs_prio;
    int es_prio;
    int num_ps_gprs;
    int num_vs_gprs;
    int num_gs_gprs;
    int num_es_gprs;
    int num_temp_gprs;
    int num_ps_threads;
    int num_vs_threads;
    int num_gs_threads;
    int num_es_threads;
    int num_ps_stack_entries;
    int num_vs_stack_entries;
    int num_gs_stack_entries;
    int num_es_stack_entries;
} sq_config_t;

/* Color buffer / render target */
typedef struct {
    int id;
    int w;
    int h;
    uint64_t base;
    int format;
    int endian;
    int array_mode;						// tiling
    int number_type;
    int read_size;
    int comp_swap;
    int tile_mode;
    int blend_clamp;
    int clear_color;
    int blend_bypass;
    int blend_float32;
    int simple_float;
    int round_mode;
    int tile_compact;
    int source_format;
} cb_config_t;

/* Depth buffer */
typedef struct {
    int w;
    int h;
    uint64_t base;
    int format;
    int read_size;
    int array_mode;						// tiling
    int tile_surface_en;
    int tile_compact;
    int zrange_precision;
} db_config_t;

/* Shader */
typedef struct {
    uint64_t shader_addr;
    int num_gprs;
    int stack_size;
    int dx10_clamp;
    int prime_cache_pgm_en;
    int prime_cache_on_draw;
    int fetch_cache_lines;
    int prime_cache_en;
    int prime_cache_on_const;
    int clamp_consts;
    int export_mode;
    int uncached_first_inst;
} shader_config_t;

/* Vertex buffer / vtx resource */
typedef struct {
    int id;
    uint64_t vb_addr;
    uint32_t vtx_num_entries;
    uint32_t vtx_size_dw;
    int clamp_x;
    int format;
    int num_format_all;
    int format_comp_all;
    int srf_mode_all;
    int endian;
    int mem_req_size;
} vtx_resource_t;

/* Texture resource */
typedef struct {
    int id;
    int w;
    int h;
    int pitch;
    int depth;
    int dim;
    int tile_mode;
    int tile_type;
    int format;
    uint64_t base;
    uint64_t mip_base;
    int format_comp_x;
    int format_comp_y;
    int format_comp_z;
    int format_comp_w;
    int num_format_all;
    int srf_mode_all;
    int force_degamma;
    int endian;
    int request_size;
    int dst_sel_x;
    int dst_sel_y;
    int dst_sel_z;
    int dst_sel_w;
    int base_level;
    int last_level;
    int base_array;
    int last_array;
    int mpeg_clamp;
    int perf_modulation;
    int interlaced;
} tex_resource_t;

/* Texture sampler */
typedef struct {
    int				id;
    /* Clamping */
    int				clamp_x, clamp_y, clamp_z;
    int		       		border_color;
    /* Filtering */
    int				xy_mag_filter, xy_min_filter;
    int				z_filter;
    int				mip_filter;
    bool_t			high_precision_filter;	/* ? */
    int				perf_mip;		/* ? 0-7 */
    int				perf_z;			/* ? 3 */
    /* LoD selection */
    int				min_lod, max_lod;	/* 0-0x3ff */
    int                         lod_bias;		/* 0-0xfff (signed?) */
    int                         lod_bias2;		/* ? 0-0xfff (signed?) */
    bool_t			lod_uses_minor_axis;	/* ? */
    /* Other stuff */
    bool_t			point_sampling_clamp;	/* ? */
    bool_t			tex_array_override;	/* ? */
    bool_t                      mc_coord_truncate;	/* ? */
    bool_t			force_degamma;		/* ? */
    bool_t			fetch_4;		/* ? */
    bool_t			sample_is_pcf;		/* ? */
    bool_t			type;			/* ? */
    int				depth_compare;		/* only depth textures? */
    int				chroma_key;
} tex_sampler_t;

/* Draw command */
typedef struct {
    uint32_t prim_type;
    uint32_t vgt_draw_initiator;
    uint32_t index_type;
    uint32_t num_instances;
    uint32_t num_indices;
} draw_config_t;

#define E32(ib, dword)                                                  \
do {                                                                    \
    uint32_t *ib_head = (pointer)(char*)(ib)->address;			\
    ib_head[(ib)->used >> 2] = (dword);					\
    (ib)->used += 4;							\
} while (0)

#define EFLOAT(ib, val)							\
do {								        \
    union { float f; uint32_t d; } a;                                   \
    a.f = (val);								\
    E32((ib), a.d);							\
} while (0)

#define PACK3(ib, cmd, num)	       					\
do {                                                                    \
    E32((ib), RADEON_CP_PACKET3 | ((cmd) << 8) | ((((num) - 1) & 0x3fff) << 16)); \
} while (0)

/* write num registers, start at reg */
/* If register falls in a special area, special commands are issued */
#define PACK0(ib, reg, num)                                             \
do {                                                                    \
    if ((reg) >= SET_CONFIG_REG_offset && (reg) < SET_CONFIG_REG_end) {	\
	PACK3((ib), IT_SET_CONFIG_REG, (num) + 1);			\
        E32(ib, ((reg) - SET_CONFIG_REG_offset) >> 2);                  \
    } else if ((reg) >= SET_CONTEXT_REG_offset && (reg) < SET_CONTEXT_REG_end) { \
        PACK3((ib), IT_SET_CONTEXT_REG, (num) + 1);			\
	E32(ib, ((reg) - 0x28000) >> 2);				\
    } else if ((reg) >= SET_ALU_CONST_offset && (reg) < SET_ALU_CONST_end) { \
	PACK3((ib), IT_SET_ALU_CONST, (num) + 1);			\
	E32(ib, ((reg) - SET_ALU_CONST_offset) >> 2);			\
    } else if ((reg) >= SET_RESOURCE_offset && (reg) < SET_RESOURCE_end) { \
	PACK3((ib), IT_SET_RESOURCE, num + 1);				\
	E32((ib), ((reg) - SET_RESOURCE_offset) >> 2);			\
    } else if ((reg) >= SET_SAMPLER_offset && (reg) < SET_SAMPLER_end) { \
	PACK3((ib), IT_SET_SAMPLER, (num) + 1);				\
	E32((ib), (reg - SET_SAMPLER_offset) >> 2);			\
    } else if ((reg) >= SET_CTL_CONST_offset && (reg) < SET_CTL_CONST_end) { \
	PACK3((ib), IT_SET_CTL_CONST, (num) + 1);			\
	E32((ib), ((reg) - SET_CTL_CONST_offset) >> 2);		\
    } else if ((reg) >= SET_LOOP_CONST_offset && (reg) < SET_LOOP_CONST_end) { \
	PACK3((ib), IT_SET_LOOP_CONST, (num) + 1);			\
	E32((ib), ((reg) - SET_LOOP_CONST_offset) >> 2);		\
    } else if ((reg) >= SET_BOOL_CONST_offset && (reg) < SET_BOOL_CONST_end) { \
	PACK3((ib), IT_SET_BOOL_CONST, (num) + 1);			\
	E32((ib), ((reg) - SET_BOOL_CONST_offset) >> 2);		\
    } else {								\
	E32((ib), CP_PACKET0 ((reg), (num) - 1));			\
    }									\
} while (0)

/* write a single register */
#define EREG(ib, reg, val)                                              \
do {								        \
    PACK0((ib), (reg), 1);						\
    E32((ib), (val));							\
} while (0)

void R600CPFlushIndirect(ScrnInfoPtr pScrn, drmBufPtr ib);
void R600IBDiscard(ScrnInfoPtr pScrn, drmBufPtr ib);

uint64_t
upload (ScrnInfoPtr pScrn, void *shader, int size, int offset);
void
wait_3d_idle_clean(ScrnInfoPtr pScrn, drmBufPtr ib);
void
wait_3d_idle(ScrnInfoPtr pScrn, drmBufPtr ib);
void
start_3d(ScrnInfoPtr pScrn, drmBufPtr ib);
void
set_render_target(ScrnInfoPtr pScrn, drmBufPtr ib, cb_config_t *cb_conf);
void
cp_set_surface_sync(ScrnInfoPtr pScrn, drmBufPtr ib, uint32_t sync_type, uint32_t size, uint64_t mc_addr);
void
cp_wait_vline_sync(ScrnInfoPtr pScrn, drmBufPtr ib, PixmapPtr pPix, int crtc, int start, int stop, Bool enable);
void
fs_setup(ScrnInfoPtr pScrn, drmBufPtr ib, shader_config_t *fs_conf);
void
vs_setup(ScrnInfoPtr pScrn, drmBufPtr ib, shader_config_t *vs_conf);
void
ps_setup(ScrnInfoPtr pScrn, drmBufPtr ib, shader_config_t *ps_conf);
void
set_alu_consts(ScrnInfoPtr pScrn, drmBufPtr ib, int offset, int count, float *const_buf);
void
set_bool_const(ScrnInfoPtr pScrn, drmBufPtr ib, int offset, uint32_t val);
void
set_vtx_resource(ScrnInfoPtr pScrn, drmBufPtr ib, vtx_resource_t *res);
void
set_tex_resource(ScrnInfoPtr pScrn, drmBufPtr ib, tex_resource_t *tex_res);
void
set_tex_sampler (ScrnInfoPtr pScrn, drmBufPtr ib, tex_sampler_t *s);
void
set_screen_scissor(ScrnInfoPtr pScrn, drmBufPtr ib, int x1, int y1, int x2, int y2);
void
set_vport_scissor(ScrnInfoPtr pScrn, drmBufPtr ib, int id, int x1, int y1, int x2, int y2);
void
set_generic_scissor(ScrnInfoPtr pScrn, drmBufPtr ib, int x1, int y1, int x2, int y2);
void
set_window_scissor(ScrnInfoPtr pScrn, drmBufPtr ib, int x1, int y1, int x2, int y2);
void
set_clip_rect(ScrnInfoPtr pScrn, drmBufPtr ib, int id, int x1, int y1, int x2, int y2);
void
set_default_state(ScrnInfoPtr pScrn, drmBufPtr ib);
void
draw_immd(ScrnInfoPtr pScrn, drmBufPtr ib, draw_config_t *draw_conf, uint32_t *indices);
void
draw_auto(ScrnInfoPtr pScrn, drmBufPtr ib, draw_config_t *draw_conf);

#endif
