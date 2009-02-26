#ifndef __R600_STATE_H__
#define __R600_STATE_H__

#include "xf86drm.h"

typedef int bool_t;

/* seriously ?! @#$%% */
# define uint32_t CARD32
# define uint64_t CARD64

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

inline void e32(drmBufPtr ib, uint32_t dword);
inline void efloat(drmBufPtr ib, float f);
inline void pack3(drmBufPtr ib, int cmd, unsigned num);
inline void pack0 (drmBufPtr ib, uint32_t reg, int num);
inline void ereg (drmBufPtr ib, uint32_t reg, uint32_t val);
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
