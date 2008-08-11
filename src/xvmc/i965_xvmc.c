/*
 * Copyright © 2008 Intel Corporation
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
 *
 */
#include "i965_xvmc.h"
#include "i810_reg.h"
#include "brw_defines.h"
#include "brw_structs.h"
#include "intel_batchbuffer.h"
#include "i965_hwmc.h"
#define BATCH_STRUCT(x) intelBatchbufferData(&x, sizeof(x), 0)
#define URB_SIZE	256	/* XXX */
enum interface {
    INTRA_INTERFACE,		/* non field intra */
    NULL_INTERFACE,		/* fill with white, do nothing, for debug */
    FORWARD_INTERFACE,		/* non field forward predict */
    BACKWARD_INTERFACE,		/* non field backward predict */
    F_B_INTERFACE,		/* non field forward and backward predict */
    FIELD_INTRA_INTERFACE,	/* field intra */
    FIELD_FORWARD_INTERFACE,	/* field forward predict */
    FIELD_BACKWARD_INTERFACE,	/* field backward predict */
    FIELD_F_B_INTERFACE		/* field forward and backward predict */
};

static const uint32_t ipicture_kernel_static[][4] = {
	#include "ipicture.g4b"
};
static const uint32_t null_kernel_static[][4] = {
	#include "null.g4b"
};
static const uint32_t forward_kernel_static[][4] = {
	#include "forward.g4b"
};
static const uint32_t backward_kernel_static[][4] = {
	#include "backward.g4b"
};
static const uint32_t f_b_kernel_static[][4] = {
	#include "f_b.g4b"
}; 

#define ALIGN(i,m)    (((i) + (m) - 1) & ~((m) - 1))

#define	VFE_GENERIC_MODE	0x0
#define	VFE_VLD_MODE		0x1
#define VFE_IS_MODE		0x2
#define VFE_AVC_MC_MODE		0x4
#define VFE_AVC_IT_MODE		0x7
#define VFE_VC1_IT_MODE		0x7

#define MAX_SURFACE_NUM	10
#define DESCRIPTOR_NUM 12

struct media_state {
	unsigned long state_base;
	void 	      *state_ptr;
	unsigned int  binding_table_entry_count;
	unsigned long vfe_state_offset;
	unsigned long interface_descriptor_offset[DESCRIPTOR_NUM];
	unsigned long ipicture_kernel_offset;
	unsigned long forward_kernel_offset;
	unsigned long backward_kernel_offset;
	unsigned long f_b_kernel_offset;
	unsigned long ipicture_field_kernel_offset;
	unsigned long forward_field_kernel_offset;
	unsigned long backward_field_kernel_offset;
	unsigned long f_b_field_kernel_offset;
	unsigned long null_kernel_offset;
	unsigned long surface_offsets[MAX_SURFACE_NUM];
	unsigned long binding_table_offset;
};
struct media_state media_state;

static int map_buffer(struct  drm_memory_block *mem)
{
    return (drmMap(xvmc_driver->fd, 
		mem->handle, mem->size, &mem->ptr));
}

static void unmap_buffer(struct  drm_memory_block *mem)
{
    drmUnmap(mem->ptr, mem->size);
}

static Status destroy_context(Display *display, XvMCContext *context)
{
    struct i965_xvmc_context *private_context;
    private_context = context->privData;
    unmap_buffer(&private_context->static_buffer);
    Xfree(private_context);
    return Success;
}

static Status create_surface(Display *display,
        XvMCContext *context, XvMCSurface *surface, int priv_count,
        CARD32 *priv_data)
{
    struct i965_xvmc_surface *priv_surface = 
	(struct i965_xvmc_surface *)priv_data;
    if (map_buffer(&priv_surface->buffer))
	return BadAlloc;
    surface->privData = priv_data;
    return Success;
}

static Status destroy_surface(Display *display, XvMCSurface *surface)
{
    struct i965_xvmc_surface *priv_surface = 
	surface->privData;
    unmap_buffer(&priv_surface->buffer);
    return Success;
}

static void flush()
{
    struct brw_mi_flush flush;
    memset(&flush, 0, sizeof(flush));
    flush.opcode = CMD_MI_FLUSH;
    BATCH_STRUCT(flush);
}

static void clear_sf_state()
{
    struct brw_sf_unit_state sf;
    memset(&sf, 0, sizeof(sf));
    /* TODO */
}


/* urb fence must be aligned to cacheline */
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

/* setup urb layout for media */
static void urb_layout()
{
    BATCH_LOCALS;
    align_urb_fence();
    BEGIN_BATCH(3);
    OUT_BATCH(BRW_URB_FENCE |
	    UF0_VFE_REALLOC |
	    UF0_CS_REALLOC |
	    1);
    OUT_BATCH(0);
    OUT_BATCH(((URB_SIZE)<< UF2_VFE_FENCE_SHIFT) |	/* VFE_SIZE */
	    ((URB_SIZE)<< UF2_CS_FENCE_SHIFT));		/* CS_SIZE is 0 */
    ADVANCE_BATCH();
}

/* clear previous urb layout */
static void clear_urb_state()
{
    BATCH_LOCALS;
    align_urb_fence();
    BEGIN_BATCH(3);
    OUT_BATCH(BRW_URB_FENCE |
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
	    (0 << UF2_SF_FENCE_SHIFT));
    ADVANCE_BATCH();
}

static void media_state_pointers(struct media_state *media_state)
{
    BATCH_LOCALS;
    BEGIN_BATCH(3);
    OUT_BATCH(BRW_MEDIA_STATE_POINTERS|1);
    OUT_BATCH(0);
    OUT_BATCH(media_state->vfe_state_offset);
    ADVANCE_BATCH();
}

static void cs_urb_layout()
{
    BATCH_LOCALS;
    BEGIN_BATCH(2);
    OUT_BATCH(BRW_CS_URB_STATE | 0);
    OUT_BATCH((0 << 4) |    /* URB Entry Allocation Size */
	      (0 << 0));    /* Number of URB Entries */
    ADVANCE_BATCH();
}

/* setup 2D surface for media_read or media_write 
 */
static void setup_media_surface(struct media_state *media_state,
	int surface_num, unsigned long offset, int w, int h)
{
    struct brw_surface_state *ss;
    ss = media_state->state_ptr +
	(media_state->surface_offsets[surface_num] - media_state->state_base);
    memset(ss, 0, sizeof(struct brw_surface_state));
    ss->ss0.surface_type = BRW_SURFACE_2D;
    ss->ss0.surface_format = BRW_SURFACEFORMAT_R8_SINT;
    ss->ss1.base_addr = offset;
    ss->ss2.width = w - 1;
    ss->ss2.height = h - 1;
    ss->ss3.pitch = w - 1;
}

static void setup_surfaces(struct media_state *media_state, 
	unsigned long dst_offset, unsigned long past_offset, 
	unsigned long future_offset, 
	int w, int h)
{
    setup_media_surface(media_state, 0, dst_offset, w, h);
    setup_media_surface(media_state, 1, dst_offset+w*h, w/2, h/2);
    setup_media_surface(media_state, 2, dst_offset+w*h + w*h/4, w/2, h/2);
    if (past_offset) {
	setup_media_surface(media_state, 4, past_offset, w, h);
	setup_media_surface(media_state, 5, past_offset+w*h, w/2, h/2);
	setup_media_surface(media_state, 6, past_offset+w*h + w*h/4, w/2, h/2);
    }
    if (future_offset) {
	setup_media_surface(media_state, 7, future_offset, w, h);
	setup_media_surface(media_state, 8, future_offset+w*h, w/2, h/2);
	setup_media_surface(media_state, 9, future_offset+w*h + w*h/4, w/2, h/2);
    }
}
/* BUFFER SURFACE has a strange format
 * the size of the surface is in part of w h and d component
 */

static void setup_blocks(struct media_state *media_state, 
	unsigned long offset, unsigned int block_size)
{
    union element{
		struct {
			unsigned int w:7;
			unsigned int h:13;
			unsigned int d:7;
			unsigned int pad:7;
		}whd;
		unsigned int size;	
    }e;
    struct brw_surface_state *ss;
    ss = media_state->state_ptr +
	(media_state->surface_offsets[3] - media_state->state_base);
    memset(ss, 0, sizeof(struct brw_surface_state));
    ss->ss0.surface_type = BRW_SURFACE_BUFFER;
    ss->ss0.surface_format = BRW_SURFACEFORMAT_R8_UINT;
    ss->ss1.base_addr = offset;
    e.size = block_size - 1;
    ss->ss2.width = e.whd.w;
    ss->ss2.height = e.whd.h;
    ss->ss3.depth = e.whd.d;
    ss->ss3.pitch = block_size - 1;
}

/* setup state base address */
static void state_base_address()
{
    BATCH_LOCALS;
    BEGIN_BATCH(6);
    OUT_BATCH(BRW_STATE_BASE_ADDRESS | 4);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY); 
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
    ADVANCE_BATCH();
}

/* select media pipeline */
static void pipeline_select()
{
    BATCH_LOCALS;
    BEGIN_BATCH(1);
    OUT_BATCH(BRW_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA);
    ADVANCE_BATCH();
}

/* kick media object to gpu */
static void send_media_object(XvMCMacroBlock *mb, enum interface interface)
{
    BATCH_LOCALS;
    BEGIN_BATCH(12);
    OUT_BATCH(BRW_MEDIA_OBJECT|10);
    OUT_BATCH(interface);
    OUT_BATCH(0);
    OUT_BATCH(0);
    OUT_BATCH(mb->x<<4);
    OUT_BATCH(mb->y<<4);
    OUT_BATCH(2*(mb->index<<6));
    OUT_BATCH(mb->coded_block_pattern);
    OUT_BATCH(mb->PMV[0][0][0]);
    OUT_BATCH(mb->PMV[0][0][1]);
    OUT_BATCH(mb->PMV[0][1][0]);
    OUT_BATCH(mb->PMV[0][1][1]);
    ADVANCE_BATCH();
}

/* do not use vertex cache for media object indirect data*/
static void vertex_cache()
{
    BATCH_LOCALS;
    BEGIN_BATCH(3);
    OUT_BATCH((0x22<<23)|1);
    OUT_BATCH(0x2124);
    OUT_BATCH(0x10000000);
    ADVANCE_BATCH();
}

static void binding_tables(struct media_state *media_state)
{
    unsigned int *binding_table;
    int i;
    binding_table = media_state->state_ptr +
	(media_state->binding_table_offset - media_state->state_base);
    for (i = 0; i < MAX_SURFACE_NUM; i++)
	binding_table[i] = media_state->surface_offsets[i];
}

static void media_kernels(struct media_state *media_state)
{
  	void *kernel; 
#define LOAD_KERNEL(name) kernel = media_state->state_ptr +\
	(media_state->name##_kernel_offset - media_state->state_base);\
	memcpy(kernel, name##_kernel_static, sizeof(name##_kernel_static));
	LOAD_KERNEL(ipicture);
	LOAD_KERNEL(null);
	LOAD_KERNEL(forward);
	LOAD_KERNEL(backward);
	LOAD_KERNEL(f_b);
}

static void setup_interface(struct media_state *media_state, 
	enum interface interface, unsigned int kernel_offset)
{
    struct brw_interface_descriptor *desc;
    desc = media_state->state_ptr +
	(media_state->interface_descriptor_offset[interface] 
	 - media_state->state_base);
    memset(desc, 0, sizeof(*desc));
    desc->desc0.grf_reg_blocks = 15;
    desc->desc0.kernel_start_pointer = kernel_offset >> 6;
    desc->desc1.floating_point_mode = BRW_FLOATING_POINT_NON_IEEE_754;

    /* use same binding table for all interface
     * may change this if it affect performance
     */
    desc->desc3.binding_table_entry_count = MAX_SURFACE_NUM;
    desc->desc3.binding_table_pointer = media_state->binding_table_offset >> 5;
}

static void interface_descriptor(struct media_state *media_state)
{
    setup_interface(media_state, INTRA_INTERFACE, 
	    media_state->ipicture_kernel_offset);
    setup_interface(media_state, NULL_INTERFACE, 
	    media_state->null_kernel_offset);
    setup_interface(media_state, FORWARD_INTERFACE, 
	    media_state->forward_kernel_offset);
    setup_interface(media_state, BACKWARD_INTERFACE, 
	    media_state->backward_kernel_offset);
    setup_interface(media_state, F_B_INTERFACE, 
	    media_state->f_b_kernel_offset);
}

static void vfe_state(struct media_state *media_state)
{
	struct brw_vfe_state *state;
	state = media_state->state_ptr +
	    (media_state->vfe_state_offset - media_state->state_base);
	memset(state, 0, sizeof(*state));
	/* no scratch space */
	state->vfe1.vfe_mode = VFE_GENERIC_MODE;
	state->vfe1.num_urb_entries = 1; 
	/* XXX TODO */
	/* should carefully caculate those values for performance */
	state->vfe1.urb_entry_alloc_size = 2; 
	state->vfe1.max_threads = 15; 
	state->vfe2.interface_descriptor_base = 
		media_state->interface_descriptor_offset[0] >> 4;
}

static void calc_state_layouts(struct media_state *media_state)
{
    int i;
    media_state->vfe_state_offset = ALIGN(media_state->state_base, 64);
    media_state->interface_descriptor_offset[0] = 
	ALIGN(media_state->vfe_state_offset + sizeof(struct brw_vfe_state), 64);
    for (i = 1; i < DESCRIPTOR_NUM; i++)
	media_state->interface_descriptor_offset[i] = 
	    media_state->interface_descriptor_offset[i-1]
	    + sizeof(struct brw_interface_descriptor);
    media_state->binding_table_offset = 
	ALIGN(media_state->interface_descriptor_offset[DESCRIPTOR_NUM - 1]
		+ sizeof(struct brw_interface_descriptor), 64);
    media_state->surface_offsets[0] = 
	ALIGN(media_state->binding_table_offset
		+ 4*media_state->binding_table_entry_count , 32);
    for (i = 1; i < MAX_SURFACE_NUM; i++)
	media_state->surface_offsets[i] = 
	    ALIGN(media_state->surface_offsets[i - 1] 
		    + sizeof(struct brw_surface_state) , 32);
    media_state->ipicture_kernel_offset = 
	ALIGN(media_state->surface_offsets[MAX_SURFACE_NUM - 1] 
		+ sizeof(struct brw_surface_state) , 64);
    media_state->forward_kernel_offset = 
	ALIGN(media_state->ipicture_kernel_offset + 
		sizeof(ipicture_kernel_static), 64);
    media_state->backward_kernel_offset = 
	ALIGN(media_state->forward_kernel_offset + 
		sizeof(forward_kernel_static), 64);
    media_state->f_b_kernel_offset = 
	ALIGN(media_state->backward_kernel_offset + 
		sizeof(backward_kernel_static), 64);
    media_state->null_kernel_offset =
	ALIGN(media_state->f_b_kernel_offset +
		sizeof(f_b_kernel_static), 64);
}


static Status render_surface(Display *display, 
	XvMCContext *context,
	unsigned int picture_structure,
	XvMCSurface *target_surface,
	XvMCSurface *past_surface,
	XvMCSurface *future_surface,
	unsigned int flags,
	unsigned int num_macroblocks,
	unsigned int first_macroblock,
	XvMCMacroBlockArray *macroblock_array,
	XvMCBlockArray *blocks)
{

    intel_xvmc_context_ptr intel_ctx;
    int i;
    struct i965_xvmc_context *i965_ctx;
    XvMCMacroBlock *mb;
    struct i965_xvmc_surface *priv_target_surface = 
	target_surface->privData;
    struct i965_xvmc_surface *priv_past_surface = 
	past_surface?past_surface->privData:0;
    struct i965_xvmc_surface *priv_future_surface = 
	future_surface?future_surface->privData:0;

    intel_ctx = intel_xvmc_find_context(context->context_id);
    i965_ctx = context->privData;
    if (!intel_ctx) {
	XVMC_ERR("Can't find intel xvmc context\n");
	return BadValue;
    }

    setup_surfaces(&media_state, 
	    priv_target_surface->buffer.offset, 
	    past_surface? priv_past_surface->buffer.offset:0, 
	    future_surface?priv_future_surface->buffer.offset:0, 
	    context->width, context->height);

    /* copy correction data */
    if(map_buffer(&i965_ctx->blocks))
	return BadAlloc;
    for (i = first_macroblock; 
	    i < num_macroblocks + first_macroblock; i++) {
	short *p = i965_ctx->blocks.ptr;
	mb = &macroblock_array->macro_blocks[i];
	memcpy(&p[(mb->index<<6)], 
		&blocks->blocks[(mb->index<<6)], 
		mb_bytes_420[mb->coded_block_pattern]);
    }
    unmap_buffer(&i965_ctx->blocks);

    {
	LOCK_HARDWARE(intel_ctx->hw_context);
	vertex_cache();
	state_base_address();
	flush();	
	clear_sf_state();
	clear_urb_state();
	pipeline_select();
	urb_layout();	
	media_state_pointers(&media_state);
	cs_urb_layout();
	for (i = first_macroblock; 
		i < num_macroblocks + first_macroblock; i++) {
	    mb = &macroblock_array->macro_blocks[i];
	    if (mb->dct_type == XVMC_DCT_TYPE_FIELD) {
		/* TODO */
		XVMC_ERR("FIELD DCT not support yet\n");
		continue;
	    }
	    if ((mb->motion_type & 3) == XVMC_PREDICTION_DUAL_PRIME) {
		/* TODO */
		XVMC_ERR("DUAL PRIME not support yet\n");
		continue;
	    }
	    if (mb->macroblock_type & XVMC_MB_TYPE_INTRA) {
		send_media_object(mb, INTRA_INTERFACE);
	    } else if ((mb->macroblock_type&XVMC_MB_TYPE_MOTION_FORWARD))
	    {
		if (((mb->macroblock_type&XVMC_MB_TYPE_MOTION_BACKWARD)))
		    send_media_object(mb, F_B_INTERFACE);
		else
		    send_media_object(mb, FORWARD_INTERFACE);
	    } else if ((mb->macroblock_type&XVMC_MB_TYPE_MOTION_BACKWARD))
	    {
		send_media_object(mb, BACKWARD_INTERFACE);
	    }
	}
	intelFlushBatch(TRUE);
	UNLOCK_HARDWARE(intel_ctx->hw_context);
    }
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

static Status get_surface_status(Display *display,
        XvMCSurface *surface, int *stats)
{
	*stats = 0;
	return 0;
}

static Status create_context(Display *display, XvMCContext *context,
        int priv_count, CARD32 *priv_data)
{
    struct i965_xvmc_context *i965_ctx;
    i965_ctx = (struct i965_xvmc_context *)priv_data;
    context->privData = i965_ctx;
    if (map_buffer(&i965_ctx->static_buffer))
	return BadAlloc;
    {
	media_state.state_base = i965_ctx->static_buffer.offset;
	media_state.state_ptr = i965_ctx->static_buffer.ptr;
	media_state.binding_table_entry_count = MAX_SURFACE_NUM;
	calc_state_layouts(&media_state);
	vfe_state(&media_state);
	interface_descriptor(&media_state); 
	media_kernels(&media_state);
	setup_blocks(&media_state, 
		i965_ctx->blocks.offset, 
		6*context->width*context->height*sizeof(short));
	binding_tables(&media_state);
    }
    return Success;
}

struct _intel_xvmc_driver i965_xvmc_mc_driver = {
    .type               = XVMC_I965_MPEG2_MC,
    .create_context     = create_context,
    .destroy_context    = destroy_context,
    .create_surface     = create_surface,
    .destroy_surface    = destroy_surface,
    .render_surface     = render_surface,
    .put_surface        = put_surface,
    .get_surface_status = get_surface_status,
};

