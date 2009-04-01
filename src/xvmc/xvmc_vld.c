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

static struct media_state {
    unsigned long state_base;
    void 	  *state_ptr;
    unsigned long vld_state_offset;
    unsigned long vfe_state_offset;
    unsigned long interface_descriptor_offsets[16];
    unsigned long kernel_offsets[MEDIA_KERNEL_NUM];
    unsigned long cs_offset;
    unsigned long surface_state_offsets[I965_MAX_SURFACES];
    unsigned long binding_table_offset;
} media_state;

static int map_buffer(struct drm_memory_block *mem)
{
    return drmMap(xvmc_driver->fd, mem->handle, mem->size, &mem->ptr);
}
static void unmap_buffer(struct drm_memory_block *mem)
{
    drmUnmap(mem->ptr, mem->size);
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

static void calc_state_layout()
{
  int i;
  media_state.vld_state_offset = media_state.state_base;
  media_state.vfe_state_offset = 
      ALIGN(media_state.vld_state_offset + sizeof(struct brw_vld_state), 64);
  media_state.interface_descriptor_offsets[0] =
      ALIGN(media_state.vfe_state_offset + sizeof(struct brw_vfe_state), 64);
  for (i = 1; i < 16; i++)
      media_state.interface_descriptor_offsets[i] =
	  media_state.interface_descriptor_offsets[i - 1] 
	  + sizeof(struct brw_interface_descriptor);
  media_state.binding_table_offset = 
	  ALIGN(media_state.interface_descriptor_offsets[15] + 
	  + sizeof(struct brw_interface_descriptor), 64);
  media_state.surface_state_offsets[0] = ALIGN(media_state.binding_table_offset 
	  + 4*I965_MAX_SURFACES, 32);
  for (i = 1; i < I965_MAX_SURFACES; i++)
      media_state.surface_state_offsets[i] = 
	  ALIGN(media_state.surface_state_offsets[i-1]
		  + sizeof(struct brw_surface_state), 32);

  media_state.kernel_offsets[0] = 
      ALIGN(media_state.surface_state_offsets[I965_MAX_SURFACES - 1]
	      + sizeof(struct brw_surface_state), 64);
  for (i = 1; i < MEDIA_KERNEL_NUM; i++)
      media_state.kernel_offsets[i] = 
	  ALIGN(media_state.kernel_offsets[i-1] + media_kernels[i-1].size, 64);
  media_state.cs_offset = ALIGN(media_state.kernel_offsets[MEDIA_KERNEL_NUM-1]
	  + media_kernels[MEDIA_KERNEL_NUM-1].size, 64);
}

static void *offset_to_ptr(unsigned long offset)
{
    return media_state.state_ptr + (offset - media_state.state_base);
}

static void vfe_state()
{
  struct brw_vfe_state *vfe_state;
  vfe_state = offset_to_ptr(media_state.vfe_state_offset);
  memset(vfe_state, 0, sizeof(*vfe_state));
  vfe_state->vfe0.extend_vfe_state_present = 1;
  vfe_state->vfe1.vfe_mode = VFE_VLD_MODE;
  vfe_state->vfe1.num_urb_entries = 1;
  vfe_state->vfe1.children_present = 0;
  vfe_state->vfe1.urb_entry_alloc_size = 2;
  vfe_state->vfe1.max_threads = 31;

  vfe_state->vfe2.interface_descriptor_base = 
      media_state.interface_descriptor_offsets[0] >> 4;
}

static void interface_descriptor()
{
    int i;
    struct brw_interface_descriptor *desc;
    for (i = 0; i < MEDIA_KERNEL_NUM; i++) {
	desc = offset_to_ptr(media_state.interface_descriptor_offsets[i]);
	memset(desc, 0, sizeof(*desc));
	desc->desc0.grf_reg_blocks = 15;
	desc->desc0.kernel_start_pointer = media_state.kernel_offsets[i] >> 6;

	desc->desc1.const_urb_entry_read_offset = 0;
	desc->desc1.const_urb_entry_read_len = 30;

	desc->desc3.binding_table_entry_count = I965_MAX_SURFACES - 1;
	desc->desc3.binding_table_pointer = media_state.binding_table_offset>>5;
    }
}

static void setup_media_kernels()
{
    int i;
    void *kernel_ptr;
    for (i = 0; i < MEDIA_KERNEL_NUM; i++) {
	kernel_ptr = offset_to_ptr(media_state.kernel_offsets[i]);
	memcpy(kernel_ptr, media_kernels[i].bin, media_kernels[i].size);
    }
}

static void binding_tables()
{
   unsigned int *table;
   int i;
   table = offset_to_ptr(media_state.binding_table_offset);
   for (i = 0; i < I965_MAX_SURFACES; i++)
       table[i] = media_state.surface_state_offsets[i];
}

static void cs_init()
{
   void *buf;
   unsigned int *lib_reloc;
   int i;
   buf = offset_to_ptr(media_state.cs_offset);
   memcpy(buf + 32*4, idct_table, sizeof(idct_table));
   /* idct lib reloction */
   lib_reloc = buf + 32*20;
   for (i = 0; i < 8; i++)
       lib_reloc[i] = media_state.kernel_offsets[LIB_INTERFACE];
}

static Status create_context(Display *display, XvMCContext *context,
	int priv_count, CARD32 *priv_data)
{
    struct i965_xvmc_context *i965_ctx;
    i965_ctx = (struct i965_xvmc_context *)priv_data;
    context->privData = priv_data;
    if (map_buffer(&i965_ctx->static_buffer))
	return BadAlloc;
    if (map_buffer(&i965_ctx->slice))
	return BadAlloc;
    media_state.state_base = i965_ctx->static_buffer.offset;
    media_state.state_ptr = i965_ctx->static_buffer.ptr;
    calc_state_layout();
    vfe_state();
    interface_descriptor();
    setup_media_kernels();
    binding_tables();
    cs_init();
    return Success;
}

static Status destroy_context(Display *display, XvMCContext *context)
{
    struct i965_xvmc_context *i965_ctx;
    i965_ctx = context->privData;
    unmap_buffer(&i965_ctx->slice);
    unmap_buffer(&i965_ctx->static_buffer);
    Xfree(i965_ctx);
    return Success;
}

static Status create_surface(Display *display,
	XvMCContext *context, XvMCSurface *surface, int priv_count,
	CARD32 *priv_data)
{
    struct i965_xvmc_surface *x; 
    surface->privData = priv_data;
    x = surface->privData;
    return Success;
}
static Status destroy_surface(Display *display,
	XvMCSurface *surface)
{
    return Success;
}

static Status load_qmatrix(Display *display, XvMCContext *context,
	const XvMCQMatrix *qmx)
{
    unsigned char *qmatrix;
    qmatrix = offset_to_ptr(media_state.cs_offset);
    memcpy(qmatrix, qmx->intra_quantiser_matrix, 64);
    memcpy(qmatrix + 64, qmx->non_intra_quantiser_matrix, 64);
    return Success;
}

static Status get_surface_status(Display *display, XvMCSurface *surface,
	int *status)
{
    *status = 0;
    return Success;
}

static void vld_state(const XvMCMpegControl *control)
{
    struct brw_vld_state *vld;
    vld = offset_to_ptr(media_state.vld_state_offset);
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
}

static void setup_media_surface(int binding_table_index, 
	unsigned long offset, int w, int h)
{
    struct brw_surface_state *ss;
    ss = offset_to_ptr(media_state.surface_state_offsets[binding_table_index]);
    memset(ss, 0, sizeof(*ss)); 
    ss->ss0.surface_type = BRW_SURFACE_2D;
    ss->ss0.surface_format = BRW_SURFACEFORMAT_R8_SINT;
    ss->ss1.base_addr = offset;
    ss->ss2.width = w - 1;
    ss->ss2.height = h - 1;
    ss->ss3.pitch = w - 1;
}

static void setup_surface(struct i965_xvmc_surface *target,
	 struct i965_xvmc_surface *past,
	 struct i965_xvmc_surface *future,
	 int w, int h)
{
    unsigned long dst_offset, past_offset, future_offset;
    dst_offset = target->buffer.offset;
    setup_media_surface(0, dst_offset, w, h);
    setup_media_surface(1, dst_offset + w*h, w/2, h/2);
    setup_media_surface(2, dst_offset + w*h + w*h/4, w/2, h/2);
    if (past) {
	past_offset = past->buffer.offset;
	setup_media_surface(4, past_offset, w, h);
	setup_media_surface(5, past_offset + w*h, w/2, h/2);
	setup_media_surface(6, past_offset + w*h + w*h/4, w/2, h/2);
    }
    if (future) {
	future_offset = future->buffer.offset;
	setup_media_surface(7, future_offset, w, h);
	setup_media_surface(8, future_offset + w*h, w/2, h/2);
	setup_media_surface(9, future_offset + w*h + w*h/4, w/2, h/2);
    }
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
    intel_ctx = intel_xvmc_find_context(context->context_id);
    priv_target = target->privData;
    priv_past = past?past->privData:NULL;
    priv_future = future?future->privData:NULL;
    vld_state(control);
    setup_surface(priv_target, priv_past, priv_future, 
	    context->width, context->height);
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
    OUT_BATCH(media_state.vld_state_offset|1);
    OUT_BATCH(media_state.vfe_state_offset);
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
    OUT_BATCH(media_state.cs_offset|CS_SIZE);
    ADVANCE_BATCH();
}

static void vld_send_media_object(unsigned long slice_offset,
	int slice_len, int mb_h_pos, int mb_v_pos, int mb_bit_offset,
	int mb_count, int q_scale_code)
{
    BATCH_LOCALS;
    BEGIN_BATCH(6);
    OUT_BATCH(BRW_MEDIA_OBJECT|4);
    OUT_BATCH(0);
    OUT_BATCH(slice_len);
    OUT_BATCH(slice_offset);
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

    memcpy(i965_ctx->slice.ptr, slice, nbytes);
    intel_ctx = intel_xvmc_find_context(context->context_id);

    LOCK_HARDWARE(intel_ctx->hw_context);
    state_base_address();
    pipeline_select(&media_state);
    media_state_pointers(&media_state);
    urb_layout();	
    cs_urb_layout();
    cs_buffer();
    vld_send_media_object(i965_ctx->slice.offset, 
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

	data->surf_offset = private_surface->buffer.offset;
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
