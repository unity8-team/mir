/*
 * Copyright © 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Author:
 *    Zou Nan hai <nanhai.zou@intel.com>
 */
#include "xvmc_vld.h"
#include "i965_hwmc.h"
#include "i810_reg.h"
#include "brw_defines.h"
#include "brw_structs.h"

#ifndef ALIGN
#define ALIGN(m,n) (((m) + (n) - 1) & ~((n) - 1))
#endif

#define BATCH_STRUCT(x) intelBatchbufferData(&x, sizeof(x), 0)

#define VLD_MAX_SLICE_SIZE (32 * 1024)

#define CS_SIZE 30
#define URB_SIZE 384
/* idct table */
#define C0 23170
#define C1 22725
#define C2 21407
#define C3 19266
#define C4 16383
#define C5 12873
#define C6 8867
#define C7 4520
const uint32_t idct_table[] = {
    C4, C1, C2, C3, C4, C5, C6, C7,    		//g5
    C4, C1, C2, C3, C4, C5, C6, C7,
    C4, C3, C6,-C7,-C4,-C1,-C2,-C5,    
    C4, C3, C6,-C7,-C4,-C1,-C2,-C5,
    C4, C5,-C6,-C1,-C4, C7, C2, C3,
    C4, C5,-C6,-C1,-C4, C7, C2, C3,
    C4, C7,-C2,-C5, C4, C3,-C6,-C1,
    C4, C7,-C2,-C5, C4, C3,-C6,-C1,
    C4,-C7,-C2, C5, C4,-C3,-C6, C1,
    C4,-C7,-C2, C5, C4,-C3,-C6, C1,
    C4,-C5,-C6, C1,-C4,-C7, C2,-C3,
    C4,-C5,-C6, C1,-C4,-C7, C2,-C3,
    C4,-C3, C6, C7,-C4, C1,-C2, C5,
    C4,-C3, C6, C7,-C4, C1,-C2, C5,
    C4,-C1, C2,-C3, C4,-C5, C6,-C7,
    C4,-C1, C2,-C3, C4,-C5, C6,-C7		//g20
};
#undef C0 
#undef C1 
#undef C2 
#undef C3 
#undef C4 
#undef C5 
#undef C6 
#undef C7 

enum interface {
    INTRA_INTERFACE = 0,
    FORWARD_INTERFACE,
    BACKWARD_INTERFACE,
    F_B_INTERFACE,
    FIELD_FORWARD_INTERFACE,
    FIELD_BACKWARD_INTERFACE,
    FIELD_F_B_INTERFACE,
    LIB_INTERFACE
};

static uint32_t lib_kernel[][4] = {
   #include "shader/vld/lib.g4b"
};
static uint32_t ipicture_kernel[][4] = {
   #include "shader/vld/ipicture.g4b"
};
static uint32_t frame_forward_kernel[][4] = {
   #include "shader/vld/frame_forward.g4b"
};
static uint32_t frame_backward_kernel[][4] = {
   #include "shader/vld/frame_backward.g4b"
};
static uint32_t frame_f_b_kernel[][4] = {
   #include "shader/vld/frame_f_b.g4b"
};
static uint32_t field_forward_kernel[][4] = {
   #include "shader/vld/field_forward.g4b"
};
static uint32_t field_backward_kernel[][4] = {
   #include "shader/vld/field_backward.g4b"
};
static uint32_t field_f_b_kernel[][4] = {
   #include "shader/vld/field_f_b.g4b"
};

struct media_kernel {
   uint32_t (*bin)[4];
   int size;
}media_kernels[] = {
    {ipicture_kernel, sizeof(ipicture_kernel)},
    {frame_forward_kernel, sizeof(frame_forward_kernel)},
    {frame_backward_kernel, sizeof(frame_backward_kernel)},
    {frame_f_b_kernel, sizeof(frame_f_b_kernel)},
    {field_forward_kernel, sizeof(field_forward_kernel)},
    {field_backward_kernel, sizeof(field_backward_kernel)},
    {field_f_b_kernel, sizeof(field_f_b_kernel)},
    {lib_kernel, sizeof(lib_kernel)}
};

#define MEDIA_KERNEL_NUM (sizeof(media_kernels)/sizeof(media_kernels[0]))

struct media_kernel_obj {
    dri_bo *bo;
};

struct interface_descriptor_obj {
   dri_bo *bo;
   struct media_kernel_obj kernels[MEDIA_KERNEL_NUM];
};

struct vfe_state_obj {
   dri_bo *bo;
   struct interface_descriptor_obj interface;
};

struct vld_state_obj {
   dri_bo *bo;
};

struct surface_obj {
     dri_bo *bo; 
};

struct surface_state_obj {
      struct surface_obj surface; 
      dri_bo *bo;
};

struct binding_table_obj {
    dri_bo *bo;
    struct surface_state_obj surface_states[I965_MAX_SURFACES];
};

struct slice_data_obj {
    dri_bo *bo;
};

struct cs_state_obj {
    dri_bo *bo;
};

static struct media_state {
    struct vfe_state_obj vfe_state;
    struct vld_state_obj vld_state;
    struct binding_table_obj binding_table;
    struct cs_state_obj cs_object;
    struct slice_data_obj slice_data;
} media_state;

/* XvMCQMatrix * 2 + idct_table + 8 * kernel offset pointer */
#define CS_OBJECT_SIZE (32*20 + sizeof(unsigned int) * 8)
static int free_object(struct media_state *s)
{
    int i;
#define FREE_ONE_BO(bo) \
    if (bo) \
        drm_intel_bo_unreference(bo)
    FREE_ONE_BO(s->vfe_state.bo);
    FREE_ONE_BO(s->vfe_state.interface.bo);
    for (i = 0; i < MEDIA_KERNEL_NUM; i++)
        FREE_ONE_BO(s->vfe_state.interface.kernels[i].bo);
    FREE_ONE_BO(s->binding_table.bo);
    for (i = 0; i < I965_MAX_SURFACES; i++)
        FREE_ONE_BO(s->binding_table.surface_states[i].bo);
    FREE_ONE_BO(s->slice_data.bo);
    FREE_ONE_BO(s->cs_object.bo);
    FREE_ONE_BO(s->vld_state.bo);
}

static int alloc_object(struct media_state *s)
{
    int i;

    for (i = 0; i < I965_MAX_SURFACES; i++) {
        s->binding_table.surface_states[i].bo =
            drm_intel_bo_alloc(xvmc_driver->bufmgr, "surface_state", 
 		sizeof(struct brw_surface_state), 0x1000);
        if (!s->binding_table.surface_states[i].bo)
            goto out;
    }
    return 0;
out:
    free_object(s);
    return BadAlloc;
}

static void flush()
{
#define FLUSH_STATE_CACHE  	1   
    struct brw_mi_flush f;
    memset(&f, 0, sizeof(f));
    f.opcode = CMD_MI_FLUSH;
    f.flags = (1<<FLUSH_STATE_CACHE);
    BATCH_STRUCT(f);
}

static Status vfe_state()
{
  struct brw_vfe_state tmp, *vfe_state = &tmp;
  memset(vfe_state, 0, sizeof(*vfe_state));
  vfe_state->vfe0.extend_vfe_state_present = 1;
  vfe_state->vfe1.vfe_mode = VFE_VLD_MODE;
  vfe_state->vfe1.num_urb_entries = 1;
  vfe_state->vfe1.children_present = 0;
  vfe_state->vfe1.urb_entry_alloc_size = 2;
  vfe_state->vfe1.max_threads = 31;
  vfe_state->vfe2.interface_descriptor_base =
      media_state.vfe_state.interface.bo->offset >> 4;

    if (media_state.vfe_state.bo)
        drm_intel_bo_unreference(media_state.vfe_state.bo);

    media_state.vfe_state.bo = drm_intel_bo_alloc(xvmc_driver->bufmgr,
        "vfe state", sizeof(struct brw_vfe_state), 0x1000);
    if (!media_state.vfe_state.bo)
        return BadAlloc;

    drm_intel_bo_subdata(media_state.vfe_state.bo, 0, sizeof(tmp), &tmp);

    drm_intel_bo_emit_reloc(media_state.vfe_state.bo,
	offsetof(struct brw_vfe_state, vfe2),
	media_state.vfe_state.interface.bo, 0,
	I915_GEM_DOMAIN_INSTRUCTION, 0);
    return Success;
}

static Status interface_descriptor()
{
    int i;
    struct brw_interface_descriptor tmp, *desc = &tmp;

    if (media_state.vfe_state.interface.bo)
        drm_intel_bo_unreference(media_state.vfe_state.interface.bo);

    media_state.vfe_state.interface.bo = drm_intel_bo_alloc(xvmc_driver->bufmgr,
        "interfaces", MEDIA_KERNEL_NUM*sizeof(struct brw_interface_descriptor),
	    0x1000);
    if (!media_state.vfe_state.interface.bo)
        return BadAlloc;

    for (i = 0; i < MEDIA_KERNEL_NUM; i++) {
	memset(desc, 0, sizeof(*desc));
	desc->desc0.grf_reg_blocks = 15;
	desc->desc0.kernel_start_pointer =
            media_state.vfe_state.interface.kernels[i].bo->offset >> 6;

	desc->desc1.const_urb_entry_read_offset = 0;
	desc->desc1.const_urb_entry_read_len = 30;

	desc->desc3.binding_table_entry_count = I965_MAX_SURFACES - 1;
	desc->desc3.binding_table_pointer =
            media_state.binding_table.bo->offset >> 5;

        drm_intel_bo_subdata(media_state.vfe_state.interface.bo, i*sizeof(tmp), sizeof(tmp), desc);

        drm_intel_bo_emit_reloc(
	    media_state.vfe_state.interface.bo,
	    i * sizeof(*desc) + 
	    offsetof(struct brw_interface_descriptor, desc0),
	    media_state.vfe_state.interface.kernels[i].bo,
	    desc->desc0.grf_reg_blocks,
	    I915_GEM_DOMAIN_INSTRUCTION, 0);

       drm_intel_bo_emit_reloc(
	    media_state.vfe_state.interface.bo,
	    i * sizeof(*desc) + 
	    offsetof(struct brw_interface_descriptor, desc3),
	    media_state.binding_table.bo,
	    desc->desc3.binding_table_entry_count,
	    I915_GEM_DOMAIN_INSTRUCTION, 0);
    }
    return Success;
}

static int setup_media_kernels()
{
    int i;

    for (i = 0; i < MEDIA_KERNEL_NUM; i++) {
        media_state.vfe_state.interface.kernels[i].bo =
		drm_intel_bo_alloc(xvmc_driver->bufmgr, "kernel",
			media_kernels[i].size, 0x1000);
        if (!media_state.vfe_state.interface.kernels[i].bo)
            goto out;
    }

    for (i = 0; i < MEDIA_KERNEL_NUM; i++) {
        dri_bo *bo = media_state.vfe_state.interface.kernels[i].bo;
        drm_intel_bo_subdata(bo, 0, media_kernels[i].size, media_kernels[i].bin);
    }
    return 0;
out:
    free_object(&media_state);
    return BadAlloc;
}

static Status binding_tables()
{
   unsigned int table[I965_MAX_SURFACES];
   int i;

   if (media_state.binding_table.bo)
       drm_intel_bo_unreference(media_state.binding_table.bo);
   media_state.binding_table.bo = 
	drm_intel_bo_alloc(xvmc_driver->bufmgr, "binding_table", 
		I965_MAX_SURFACES*4, 0x1000);
   if (!media_state.binding_table.bo)
       return BadAlloc;

   for (i = 0; i < I965_MAX_SURFACES; i++) {
       table[i] = media_state.binding_table.surface_states[i].bo->offset;
       drm_intel_bo_emit_reloc(media_state.binding_table.bo, 
	    i * sizeof(unsigned int),
	    media_state.binding_table.surface_states[i].bo, 0,
	    I915_GEM_DOMAIN_INSTRUCTION, 0);
   }

   drm_intel_bo_subdata(media_state.binding_table.bo, 0, sizeof(table), table);
   return Success;
}

static Status cs_init()
{
   char buf[CS_OBJECT_SIZE];
   unsigned int *lib_reloc;
   int i;

   if (media_state.cs_object.bo)
       drm_intel_bo_unreference(media_state.cs_object.bo);

   media_state.cs_object.bo = drm_intel_bo_alloc(xvmc_driver->bufmgr, "cs object", CS_OBJECT_SIZE, 64);
   if (!media_state.cs_object.bo)
       return BadAlloc;

   memcpy(buf + 32*4, idct_table, sizeof(idct_table));
   /* idct lib reloction */
   lib_reloc = (unsigned int *)(buf + 32*20);
   for (i = 0; i < 8; i++)
       lib_reloc[i] = media_state.vfe_state.interface.kernels[LIB_INTERFACE].bo->offset;
   drm_intel_bo_subdata(media_state.cs_object.bo, 32*4, 32*16 + 8*sizeof(unsigned int), buf + 32*4);

   for (i = 0; i < 8; i++)
       drm_intel_bo_emit_reloc(media_state.cs_object.bo,
           32*20 + sizeof(unsigned int) * i,
           media_state.vfe_state.interface.kernels[LIB_INTERFACE].bo, 0,
           I915_GEM_DOMAIN_INSTRUCTION, 0);

   return Success;
}

static Status create_context(Display *display, XvMCContext *context,
	int priv_count, CARD32 *priv_data)
{
    struct i965_xvmc_context *i965_ctx;
    i965_ctx = (struct i965_xvmc_context *)priv_data;
    context->privData = priv_data;

    if (alloc_object(&media_state))
        return BadAlloc;

    if (setup_media_kernels())
        return BadAlloc;
    return Success;
}

static Status destroy_context(Display *display, XvMCContext *context)
{
    struct i965_xvmc_context *i965_ctx;
    i965_ctx = context->privData;
    Xfree(i965_ctx);
    return Success;
}

#define STRIDE(w)               (w)
#define SIZE_YUV420(w, h)       (h * (STRIDE(w) + STRIDE(w >> 1)))
static Status create_surface(Display *display,
	XvMCContext *context, XvMCSurface *surface, int priv_count,
	CARD32 *priv_data)
{
    struct i965_xvmc_surface *priv_surface =
	(struct i965_xvmc_surface *)priv_data;
    size_t size = SIZE_YUV420(priv_surface->w, priv_surface->h);
    surface->privData = priv_data;
    priv_surface->bo = drm_intel_bo_alloc(xvmc_driver->bufmgr, "surface", 
	    size, 0x1000);

    return Success;
}
static Status destroy_surface(Display *display,
	XvMCSurface *surface)
{
    struct i965_xvmc_surface *priv_surface = 
	surface->privData;
    XSync(display, False);
    drm_intel_bo_unreference(priv_surface->bo);
    return Success;
}

static Status load_qmatrix(Display *display, XvMCContext *context,
	const XvMCQMatrix *qmx)
{
    Status ret;
    ret = cs_init();
    if (ret != Success)
        return ret;
    drm_intel_bo_subdata(media_state.cs_object.bo, 0, 64, qmx->intra_quantiser_matrix);
    drm_intel_bo_subdata(media_state.cs_object.bo, 64, 64, qmx->non_intra_quantiser_matrix);

    return Success;
}

static Status get_surface_status(Display *display, XvMCSurface *surface,
	int *status)
{
    *status = 0;
    return Success;
}

static Status vld_state(const XvMCMpegControl *control)
{
    struct brw_vld_state tmp, *vld = &tmp;

    if (media_state.vld_state.bo)
        drm_intel_bo_unreference(media_state.vld_state.bo);
    media_state.vld_state.bo = drm_intel_bo_alloc(xvmc_driver->bufmgr, 
	    "vld state", sizeof(struct brw_vld_state), 64);
    if (!media_state.vld_state.bo)
        return BadAlloc;

    memset(vld, 0, sizeof(*vld));
    vld->vld0.f_code_0_0 = control->FHMV_range + 1;
    vld->vld0.f_code_0_1 = control->FVMV_range + 1;
    vld->vld0.f_code_1_0 = control->BHMV_range + 1;
    vld->vld0.f_code_1_1 = control->BVMV_range + 1;
    vld->vld0.intra_dc_precision = control->intra_dc_precision;
    vld->vld0.picture_structure = control->picture_structure;
    vld->vld0.top_field_first = !!(control->flags & XVMC_TOP_FIELD_FIRST);
    vld->vld0.frame_predict_frame_dct =
	!!(control->flags & XVMC_PRED_DCT_FRAME);
    vld->vld0.concealment_motion_vector =
	!!(control->flags & XVMC_CONCEALMENT_MOTION_VECTORS);
    vld->vld0.quantizer_scale_type = !!(control->flags & XVMC_Q_SCALE_TYPE);
    vld->vld0.intra_vlc_format = !!(control->flags & XVMC_INTRA_VLC_FORMAT);
    vld->vld0.scan_order = !!(control->flags & XVMC_ALTERNATE_SCAN);

    vld->vld1.picture_coding_type = control->picture_coding_type;

    vld->desc_remap_table0.index_0 = INTRA_INTERFACE;
    vld->desc_remap_table0.index_1 = FORWARD_INTERFACE;
    vld->desc_remap_table0.index_2 = FIELD_FORWARD_INTERFACE;
    vld->desc_remap_table0.index_3 = FIELD_F_B_INTERFACE; /* dual prime */
    vld->desc_remap_table0.index_4 = BACKWARD_INTERFACE;
    vld->desc_remap_table0.index_5 = FIELD_BACKWARD_INTERFACE;
    vld->desc_remap_table0.index_6 = F_B_INTERFACE;
    vld->desc_remap_table0.index_7 = FIELD_F_B_INTERFACE;

    vld->desc_remap_table1.index_8 = INTRA_INTERFACE;
    vld->desc_remap_table1.index_9 = FORWARD_INTERFACE;
    vld->desc_remap_table1.index_10 = FIELD_FORWARD_INTERFACE;
    vld->desc_remap_table1.index_11 = FIELD_F_B_INTERFACE;
    vld->desc_remap_table1.index_12 = BACKWARD_INTERFACE;
    vld->desc_remap_table1.index_13 = FIELD_BACKWARD_INTERFACE;
    vld->desc_remap_table1.index_14 = F_B_INTERFACE;
    vld->desc_remap_table1.index_15 = FIELD_F_B_INTERFACE;

    drm_intel_bo_subdata(media_state.vld_state.bo, 0, sizeof(tmp), vld);
    return Success;
}

static Status setup_media_surface(int index, dri_bo *bo,
	unsigned long offset, int w, int h, Bool write)
{
    struct brw_surface_state tmp, *ss = &tmp;
    memset(ss, 0, sizeof(*ss)); 
    ss->ss0.surface_type = BRW_SURFACE_2D;
    ss->ss0.surface_format = BRW_SURFACEFORMAT_R8_SINT;
    ss->ss1.base_addr = offset + bo->offset;
    ss->ss2.width = w - 1;
    ss->ss2.height = h - 1;
    ss->ss3.pitch = w - 1;

    if (media_state.binding_table.surface_states[index].bo)
        drm_intel_bo_unreference(media_state.binding_table.surface_states[index].bo);

    media_state.binding_table.surface_states[index].bo =
            drm_intel_bo_alloc(xvmc_driver->bufmgr, "surface_state", 
 		sizeof(struct brw_surface_state), 0x1000);
    if (!media_state.binding_table.surface_states[index].bo)
        return BadAlloc;

    drm_intel_bo_subdata(
	    media_state.binding_table.surface_states[index].bo,
	    0, sizeof(*ss), ss);
    drm_intel_bo_emit_reloc(media_state.binding_table.surface_states[index].bo, 
	    offsetof(struct brw_surface_state, ss1),
	    bo, offset,
	    I915_GEM_DOMAIN_RENDER, write?I915_GEM_DOMAIN_RENDER:0);
    return Success;
}

static Status setup_surface(struct i965_xvmc_surface *target,
	 struct i965_xvmc_surface *past,
	 struct i965_xvmc_surface *future,
	 int w, int h)
{
    Status ret;
    ret = setup_media_surface(0, target->bo, 0, w, h, TRUE);
    if (ret != Success)
        return ret;
    ret = setup_media_surface(1, target->bo, w*h, w/2, h/2, TRUE);
    if (ret != Success)
        return ret;
    ret = setup_media_surface(2, target->bo, w*h + w*h/4, w/2, h/2, TRUE);
    if (ret != Success)
        return ret;
    if (past) {
	ret = setup_media_surface(4, past->bo, 0, w, h, FALSE);
        if (ret != Success)
            return ret;
	ret = setup_media_surface(5, past->bo, w*h, w/2, h/2, FALSE);
        if (ret != Success)
            return ret;
	ret = setup_media_surface(6, past->bo, w*h + w*h/4, w/2, h/2, FALSE);
        if (ret != Success)
            return ret;
    }
    if (future) {
	ret = setup_media_surface(7, future->bo, 0, w, h, FALSE);
        if (ret != Success)
            return ret;
	ret = setup_media_surface(8, future->bo, w*h, w/2, h/2, FALSE);
        if (ret != Success)
            return ret;
	ret = setup_media_surface(9, future->bo, w*h + w*h/4, w/2, h/2, FALSE);
        if (ret != Success)
            return ret;
    }
    return Success;
}

static  Status begin_surface(Display *display, XvMCContext *context,
	    XvMCSurface *target,
	    XvMCSurface *past,
	    XvMCSurface *future,
	    const XvMCMpegControl *control)
{
    struct i965_xvmc_contex *i965_ctx;
    struct i965_xvmc_surface *priv_target, *priv_past, *priv_future;
    intel_xvmc_context_ptr intel_ctx;
    Status ret;

    intel_ctx = intel_xvmc_find_context(context->context_id);
    priv_target = target->privData;
    priv_past = past?past->privData:NULL;
    priv_future = future?future->privData:NULL;

    ret = vld_state(control);
    if (ret != Success)
        return ret;
    ret = setup_surface(priv_target, priv_past, priv_future, 
	    context->width, context->height);
    if (ret != Success)
        return ret;
    ret = binding_tables();
    if (ret != Success)
        return ret;
    ret = interface_descriptor();
    if (ret != Success)
        return ret;
    ret = vfe_state();
    if (ret != Success)
        return ret;

    LOCK_HARDWARE(intel_ctx->hw_context);
    flush();
    UNLOCK_HARDWARE(intel_ctx->hw_context);
    return Success;
}

static Status put_slice(Display *display, XvMCContext *context, 
	unsigned char *slice, int nbytes)
{
    return Success;
}

static void state_base_address()
{
    BATCH_LOCALS;
    BEGIN_BATCH(6);
    OUT_BATCH(BRW_STATE_BASE_ADDRESS|4);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    ADVANCE_BATCH();
}

static void pipeline_select()
{
    BATCH_LOCALS;
    BEGIN_BATCH(1);
    OUT_BATCH(NEW_PIPELINE_SELECT|PIPELINE_SELECT_MEDIA);
    ADVANCE_BATCH();
}
static void media_state_pointers()
{
    BATCH_LOCALS;
    BEGIN_BATCH(3);
    OUT_BATCH(BRW_MEDIA_STATE_POINTERS|1);
    OUT_RELOC(media_state.vld_state.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 1);
    OUT_RELOC(media_state.vfe_state.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);
    ADVANCE_BATCH();
}
static void align_urb_fence()
{
    BATCH_LOCALS;
    int i, offset_to_next_cacheline;
    unsigned long batch_offset;
    BEGIN_BATCH(3);
    batch_offset = (void *)batch_ptr - xvmc_driver->alloc.ptr;
    offset_to_next_cacheline = ALIGN(batch_offset, 64) - batch_offset; 
    if (offset_to_next_cacheline <= 12 && offset_to_next_cacheline != 0) {
	for (i = 0; i < offset_to_next_cacheline/4; i++)
	    OUT_BATCH(0);
	ADVANCE_BATCH();
    }
}
static void urb_layout()
{
    BATCH_LOCALS;
    align_urb_fence();
    BEGIN_BATCH(3);
    OUT_BATCH(BRW_URB_FENCE |
	    UF0_VFE_REALLOC |
	    UF0_CS_REALLOC |
	    UF0_SF_REALLOC |
	    UF0_CLIP_REALLOC |
	    UF0_GS_REALLOC |
	    UF0_VS_REALLOC |
	    1);

    OUT_BATCH((0 << UF1_CLIP_FENCE_SHIFT) |
	    (0 << UF1_GS_FENCE_SHIFT) |
	    (0 << UF1_VS_FENCE_SHIFT));

    OUT_BATCH((0 << UF2_CS_FENCE_SHIFT) |
	    (0 << UF2_SF_FENCE_SHIFT) |
	    ((URB_SIZE - CS_SIZE - 1)<< UF2_VFE_FENCE_SHIFT) |	/* VFE_SIZE */
	    ((URB_SIZE)<< UF2_CS_FENCE_SHIFT));			/* CS_SIZE */
    ADVANCE_BATCH();
}

static void cs_urb_layout()
{
    BATCH_LOCALS;
    BEGIN_BATCH(2);
    OUT_BATCH(BRW_CS_URB_STATE | 0);
    OUT_BATCH((CS_SIZE << 4) |    /* URB Entry Allocation Size */
	    (1 << 0));    /* Number of URB Entries */
    ADVANCE_BATCH();
}

static void cs_buffer()
{
    BATCH_LOCALS;
    BEGIN_BATCH(2);
    OUT_BATCH(BRW_CONSTANT_BUFFER|0|(1<<8));
    OUT_RELOC(media_state.cs_object.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, CS_SIZE);
    ADVANCE_BATCH();
}

static void vld_send_media_object(dri_bo *bo,
	int slice_len, int mb_h_pos, int mb_v_pos, int mb_bit_offset,
	int mb_count, int q_scale_code)
{
    BATCH_LOCALS;
    BEGIN_BATCH(6);
    OUT_BATCH(BRW_MEDIA_OBJECT|4);
    OUT_BATCH(0);
    OUT_BATCH(slice_len);
    OUT_RELOC(bo, I915_GEM_DOMAIN_INSTRUCTION, 0, 0);
    OUT_BATCH((mb_h_pos<<24)|(mb_v_pos<<16)|(mb_count<<8)|(mb_bit_offset));
    OUT_BATCH(q_scale_code<<24);
    ADVANCE_BATCH();
}

static Status put_slice2(Display *display, XvMCContext *context, 
	unsigned char *slice, int nbytes, int sliceCode)
{
    unsigned int bit_buf;
    intel_xvmc_context_ptr intel_ctx;
    struct i965_xvmc_context *i965_ctx;
    int q_scale_code, mb_row;

    i965_ctx = (struct i965_xvmc_context *)context->privData;
    mb_row = *(slice - 1) - 1;
    bit_buf = (slice[0]<<24) | (slice[1]<<16) | (slice[2]<<8) | (slice[3]);

    q_scale_code = bit_buf>>27;

    if (media_state.slice_data.bo) {
        if (xvmc_driver->kernel_exec_fencing)
            drm_intel_gem_bo_unmap_gtt(media_state.slice_data.bo);
        else
            drm_intel_bo_unmap(media_state.slice_data.bo);

        drm_intel_bo_unreference(media_state.slice_data.bo);
    }
    media_state.slice_data.bo = drm_intel_bo_alloc(xvmc_driver->bufmgr, 
	    "slice data", VLD_MAX_SLICE_SIZE, 64);
    if (!media_state.slice_data.bo)
        return BadAlloc;
    if (xvmc_driver->kernel_exec_fencing)
        drm_intel_gem_bo_map_gtt(media_state.slice_data.bo);
    else
        drm_intel_bo_map(media_state.slice_data.bo, 1);

    memcpy(media_state.slice_data.bo->virtual, slice, nbytes);

    intel_ctx = intel_xvmc_find_context(context->context_id);
    LOCK_HARDWARE(intel_ctx->hw_context);
    state_base_address();
    pipeline_select(&media_state);
    media_state_pointers(&media_state);
    urb_layout();	
    cs_urb_layout();
    cs_buffer();
    vld_send_media_object(media_state.slice_data.bo,
	    nbytes, 
	    0, mb_row, 6, 127, q_scale_code);
    intelFlushBatch(TRUE);
    UNLOCK_HARDWARE(intel_ctx->hw_context);

    return Success;
}

static Status put_surface(Display *display,XvMCSurface *surface,
	Drawable draw, short srcx, short srcy,
	unsigned short srcw, unsigned short srch,
	short destx, short desty,
	unsigned short destw, unsigned short desth,
	int flags, struct intel_xvmc_command *data)
{
	struct i965_xvmc_surface *private_surface =
		surface->privData;
        uint32_t handle;

        drm_intel_bo_flink(private_surface->bo, &handle);
        data->handle = handle;
	return Success;
}

struct _intel_xvmc_driver xvmc_vld_driver = {
    .type = XVMC_I965_MPEG2_VLD,
    .create_context = create_context,
    .destroy_context = destroy_context,
    .create_surface = create_surface,
    .destroy_surface = destroy_surface,
    .load_qmatrix = load_qmatrix,
    .get_surface_status = get_surface_status,
    .begin_surface = begin_surface,
    .put_surface = put_surface,
    .put_slice = put_slice,
    .put_slice2 = put_slice2
};
