
/* public interface file */

#include <X11/X.h>
#include <X11/Xlibint.h>
#include <fourcc.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XvMC.h>
#include <X11/extensions/XvMClib.h>
#include <xf86drm.h>
#include <drm_sarea.h>

#include "intel_xvmc.h"
#include "xf86dri.h"
#include "driDrawable.h"

static struct _intel_xvmc_driver* xvmc_driver = NULL;
static int error_base;
static int event_base;

/***************************************************************************
// Function: XvMCCreateContext
// Description: Create a XvMC context for the given surface parameters.
// Arguments:
//   display - Connection to the X server.
//   port - XvPortID to use as avertised by the X connection.
//   surface_type_id - Unique identifier for the Surface type.
//   width - Width of the surfaces.
//   height - Height of the surfaces.
//   flags - one or more of the following
//      XVMC_DIRECT - A direct rendered context is requested.
//
// Notes: surface_type_id and width/height parameters must match those
//        returned by XvMCListSurfaceTypes.
// Returns: Status
***************************************************************************/
Status XvMCCreateContext(Display *display, XvPortID port,
                         int surface_type_id, int width, int height,
                         int flags, XvMCContext *context)
{
    Status ret;
    drm_sarea_t *pSAREA;
    char *curBusID;
    CARD32 *priv_data = NULL;
    struct _intel_xvmc_common *comm;
    uint magic;
    int major, minor;
    int priv_count;
    int isCapable;

    /* Verify Obvious things first */
    if (!display || !context)
        return BadValue;

    if (!(flags & XVMC_DIRECT)) {
        /* Indirect */
        XVMC_ERR("Indirect Rendering not supported! Using Direct.");
        return BadAccess;
    }

    /* Limit use to root for now */
    /* FIXME: remove it ??? */
/*
    if (geteuid()) {
        printf("Use of XvMC on i915 is currently limited to root\n");
        return BadAccess;
    }
*/
    /*
       Width, Height, and flags are checked against surface_type_id
       and port for validity inside the X server, no need to check
       here.
    */
    context->surface_type_id = surface_type_id;
    context->width = (unsigned short)((width + 15) & ~15);
    context->height = (unsigned short)((height + 15) & ~15);
    context->flags = flags;
    context->port = port;

    if (!XvMCQueryExtension(display, &event_base, &error_base)) {
        XVMC_ERR("XvMCExtension is not available!");
        return BadAccess;
    }
    ret = XvMCQueryVersion(display, &major, &minor);
    if (ret) {
        XVMC_ERR("XvMCQueryVersion Failed, unable to determine protocol version.");
	return BadAccess;
    }

    /* XXX: major and minor could be checked in future for XvMC
     * protocol capability (i.e H.264/AVC decode available)
     */

    /*
      Pass control to the X server to create a drm_context_t for us and
      validate the with/height and flags.
    */
    if ((ret = _xvmc_create_context(display, context, &priv_count, &priv_data))) {
        XVMC_ERR("Unable to create XvMC Context.");
        return ret;
    }

    comm = (struct _intel_xvmc_common *)priv_data;
    XVMC_INFO("hw xvmc type %d", comm->type);
    //XXX: how to handle different driver types
    if (xvmc_driver == NULL || xvmc_driver->type != comm->type) {
	switch (comm->type) {
	    case XVMC_I915_MPEG2_MC:
		xvmc_driver = &i915_xvmc_mc_driver;
		break;
	    case XVMC_I965_MPEG2_MC:
	    case XVMC_I945_MPEG2_VLD:
	    case XVMC_I965_MPEG2_VLD:
	    default:
		XVMC_ERR("unimplemented xvmc type %d", comm->type);
		free(priv_data);
		priv_data = NULL;
		return BadAccess;
	}
    } else {
	XVMC_ERR("wrong hw xvmc type returned\n");
	free(priv_data);
	priv_data = NULL;
	return BadAccess;
    }

    if (xvmc_driver == NULL) {
	XVMC_ERR("fail to load xvmc driver for type %d\n", comm->type);
	return BadAccess;
    }

    /* driver hook should free priv_data after return if success.
     * and set xvmc_driver->screen num */
    ret = (xvmc_driver->create_context)(display, context, priv_count, priv_data);
    if (ret) {
	XVMC_ERR("driver create context failed\n");
	free(priv_data);
	return BadAccess;
    }

#if 0
    ret = uniDRIQueryDirectRenderingCapable(display, xvmc_driver->screen,
                                            &isCapable);
    if (!ret || !isCapable) {
	XVMC_ERR("Direct Rendering is not available on this system!");
        return BadAlloc;
    }

    if (!uniDRIOpenConnection(display, xvmc_driver->screen,
                              &xvmc_driver->hsarea, &curBusID)) {
        XVMC_ERR("Could not open DRI connection to X server!");
        return BadAlloc;
    }

    strncpy(xvmc_driver->busID, curBusID, 20);
    xvmc_driver->busID[20] = '\0';
    free(curBusID);

    /* Open DRI Device */
    if((xvmc_driver->fd = drmOpen("i915", NULL)) < 0) {
        XVMC_ERR("DRM Device could not be opened.");
	(xvmc_driver->fini)();
	xvmc_driver = NULL;
        return BadAccess;
    }

    /* Get magic number */
    drmGetMagic(xvmc_driver->fd, &magic);
    // context->flags = (unsigned long)magic;

    if (!uniDRIAuthConnection(display, xvmc_driver->screen, magic)) {
	XVMC_ERR("[XvMC]: X server did not allow DRI. Check permissions.");
	(xvmc_driver->fini)();
	xvmc_driver = NULL;
        return BadAlloc;
    }

    /*
     * Map DRI Sarea. we always want it right?
     */
    if (drmMap(xvmc_driver->fd, xvmc_driver->hsarea,
               xvmc_driver->sarea_size, &xvmc_driver->sarea_address) < 0) {
        XVMC_ERR("Unable to map DRI SAREA.\n");
	(xvmc_driver->fini)();
	xvmc_driver = NULL;
        return BadAlloc;
    }

    pSAREA = (drm_sarea_t *)xvmc_driver->sarea_address;
    pI915XvMC->driHwLock = (drmLock *)&pSAREA->lock;
    pI915XvMC->sarea = SAREAPTR(pI915XvMC);
    XLockDisplay(display);
    ret = XMatchVisualInfo(display, pI915XvMC->screen,
                           (pI915XvMC->depth == 32) ? 24 : pI915XvMC->depth, TrueColor,
                           &pI915XvMC->visualInfo);
    XUnlockDisplay(display);

    if (!ret) {
	XVMC_ERR("Could not find a matching TrueColor visual.");
        free(pI915XvMC);
        context->privData = NULL;
        drmUnmap(pI915XvMC->sarea_address, pI915XvMC->sarea_size);
        return BadAlloc;
    }

    if (!uniDRICreateContext(display, pI915XvMC->screen,
                             pI915XvMC->visualInfo.visual, &pI915XvMC->id,
                             &pI915XvMC->hHWContext)) {
        XVMC_ERR("Could not create DRI context.");
        free(pI915XvMC);
        context->privData = NULL;
        drmUnmap(pI915XvMC->sarea_address, pI915XvMC->sarea_size);
        return BadAlloc;
    }

    if (NULL == (pI915XvMC->drawHash = drmHashCreate())) {
	XVMC_ERR("Could not allocate drawable hash table.");
        free(pI915XvMC);
        context->privData = NULL;
        drmUnmap(pI915XvMC->sarea_address, pI915XvMC->sarea_size);
        return BadAlloc;
    }

    if (i915_xvmc_map_buffers(pI915XvMC)) {
        i915_xvmc_unmap_buffers(pI915XvMC);
        free(pI915XvMC);
        context->privData = NULL;
        drmUnmap(pI915XvMC->sarea_address, pI915XvMC->sarea_size);
        return BadAlloc;
    }

    /* Initialize private context values */
    pI915XvMC->yStride = STRIDE(width);
    pI915XvMC->uvStride = STRIDE(width >> 1);
    pI915XvMC->haveXv = 0;
    pI915XvMC->dual_prime = 0;
    pI915XvMC->last_flip = 0;
    pI915XvMC->locked = 0;
    pI915XvMC->port = context->port;
    pthread_mutex_init(&pI915XvMC->ctxmutex, NULL);
    intelInitBatchBuffer(pI915XvMC);
    pI915XvMC->ref = 1;
#endif

    return Success;
}

/***************************************************************************
// Function: XvMCDestroyContext
// Description: Destorys the specified context.
//
// Arguments:
//   display - Specifies the connection to the server.
//   context - The context to be destroyed.
//
// Returns: Status
***************************************************************************/
Status XvMCDestroyContext(Display *display, XvMCContext *context)
{
    int ret = -1;
    if (!display || !context)
        return BadValue;

    ret = (xvmc_driver->destroy_context)(display, context);
    if (ret) {
	XVMC_ERR("destroy context fail\n");
	return BadAccess;
    }

    /* Pass Control to the X server to destroy the drm_context_t */
    //XXX move generic destroy method here
    //i915_release_resource(display,context);
    return Success;
}

/***************************************************************************
// Function: XvMCCreateSurface
***************************************************************************/
Status XvMCCreateSurface(Display *display, XvMCContext *context, XvMCSurface *surface) 
{
    int ret;
//    i915XvMCContext *pI915XvMC;
//    i915XvMCSurface *pI915Surface;
//    I915XvMCCreateSurfaceRec *tmpComm = NULL;
//    int priv_count;
//    uint *priv_data;

    if (!display || !context || !surface)
        return BadValue;

    ret = (xvmc_driver->create_surface)(display, context, surface);
    if (ret) {
	XVMC_ERR("create surface failed\n");
	return BadAccess;
    }

#if 0
    if (!(pI915XvMC = context->privData))
        return (error_base + XvMCBadContext);

    PPTHREAD_MUTEX_LOCK(pI915XvMC);
    surface->privData = (i915XvMCSurface *)malloc(sizeof(i915XvMCSurface));

    if (!(pI915Surface = surface->privData)) {
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
        return BadAlloc;
    }

    /* Initialize private values */
    pI915Surface->last_render = 0;
    pI915Surface->last_flip = 0;
    pI915Surface->yStride = pI915XvMC->yStride;
    pI915Surface->uvStride = pI915XvMC->uvStride;
    pI915Surface->width = context->width;
    pI915Surface->height = context->height;
    pI915Surface->privContext = pI915XvMC;
    pI915Surface->privSubPic = NULL;
    pI915Surface->srf.map = NULL;
    XLockDisplay(display);

    if ((ret = _xvmc_create_surface(display, context, surface,
                                    &priv_count, &priv_data))) {
        XUnlockDisplay(display);
        XVMC_ERR("Unable to create XvMCSurface.");
        free(pI915Surface);
        surface->privData = NULL;
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
        return ret;
    }

    XUnlockDisplay(display);

    if (priv_count != (sizeof(I915XvMCCreateSurfaceRec) >> 2)) {
        XVMC_ERR("_xvmc_create_surface() returned incorrect data size!");
        XVMC_INFO("\tExpected %d, got %d",
               (int)(sizeof(I915XvMCCreateSurfaceRec) >> 2), priv_count);
        _xvmc_destroy_surface(display, surface);
        free(priv_data);
        free(pI915Surface);
        surface->privData = NULL;
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
        return BadAlloc;
    }

    tmpComm = (I915XvMCCreateSurfaceRec *)priv_data;

    pI915Surface->srfNo = tmpComm->srfno;
    pI915Surface->srf.handle = tmpComm->srf.handle;
    pI915Surface->srf.offset = tmpComm->srf.offset;
    pI915Surface->srf.size = tmpComm->srf.size;
    free(priv_data);

    if (drmMap(pI915XvMC->fd,
               pI915Surface->srf.handle,
               pI915Surface->srf.size,
               (drmAddress *)&pI915Surface->srf.map) != 0) {
        _xvmc_destroy_surface(display, surface);
        free(pI915Surface);
        surface->privData = NULL;
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
        return BadAlloc;
    }

    pI915XvMC->ref++;
    PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
#endif

    return Success;
}


/***************************************************************************
// Function: XvMCDestroySurface
***************************************************************************/
Status XvMCDestroySurface(Display *display, XvMCSurface *surface) 
{
//    i915XvMCSurface *pI915Surface;
//    i915XvMCContext *pI915XvMC;

    if (!display || !surface)
        return BadValue;

    (xvmc_driver->destroy_surface)(display, surface);

#if 0
    if (!(pI915Surface = surface->privData))
        return (error_base + XvMCBadSurface);

    if (!(pI915XvMC = pI915Surface->privContext))
        return (error_base + XvMCBadSurface);

    if (pI915Surface->last_flip)
        XvMCSyncSurface(display,surface);

    if (pI915Surface->srf.map)
        drmUnmap(pI915Surface->srf.map, pI915Surface->srf.size);

    XLockDisplay(display);
    _xvmc_destroy_surface(display, surface);
    XUnlockDisplay(display);

    free(pI915Surface);
    surface->privData = NULL;
    pI915XvMC->ref--;
#endif

    return Success;
}

/***************************************************************************
// Function: XvMCCreateBlocks
***************************************************************************/
Status XvMCCreateBlocks(Display *display, XvMCContext *context,
                        unsigned int num_blocks, 
                        XvMCBlockArray *block) 
{
    if (!display || !context || !num_blocks || !block)
        return BadValue;

    memset(block, 0, sizeof(XvMCBlockArray));

    if (!(block->blocks = (short *)malloc(num_blocks << 6 * sizeof(short))))
        return BadAlloc;

    block->num_blocks = num_blocks;
    block->context_id = context->context_id;
    block->privData = NULL;

    return Success;
}

/***************************************************************************
// Function: XvMCDestroyBlocks
***************************************************************************/
Status XvMCDestroyBlocks(Display *display, XvMCBlockArray *block) 
{
    if (!display || block)
        return BadValue;

    if (block->blocks)
        free(block->blocks);

    block->context_id = 0;
    block->num_blocks = 0;
    block->blocks = NULL;
    block->privData = NULL;

    return Success;
}

/***************************************************************************
// Function: XvMCCreateMacroBlocks
***************************************************************************/
Status XvMCCreateMacroBlocks(Display *display, XvMCContext *context,
                             unsigned int num_blocks,
                             XvMCMacroBlockArray *blocks) 
{
    if (!display || !context || !blocks || !num_blocks)
        return BadValue;

    memset(blocks, 0, sizeof(XvMCMacroBlockArray));
    blocks->macro_blocks = (XvMCMacroBlock *)malloc(num_blocks * sizeof(XvMCMacroBlock));

    if (!blocks->macro_blocks)
        return BadAlloc;

    blocks->num_blocks = num_blocks;
    blocks->context_id = context->context_id;
    blocks->privData = NULL;

    return Success;
}

/***************************************************************************
// Function: XvMCDestroyMacroBlocks
***************************************************************************/
Status XvMCDestroyMacroBlocks(Display *display, XvMCMacroBlockArray *block) 
{
    if (!display || !block)
        return BadValue;
    if (block->macro_blocks)
        free(block->macro_blocks);

    block->context_id = 0;
    block->num_blocks = 0;
    block->macro_blocks = NULL;
    block->privData = NULL;

    return Success;
}

/***************************************************************************
// Function: XvMCRenderSurface
// Description: This function does the actual HWMC. Given a list of
//  macroblock structures it dispatched the hardware commands to execute
//  them. 
***************************************************************************/
Status XvMCRenderSurface(Display *display, XvMCContext *context,
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
    int ret;

    if (!display || !context || !target_surface) {
        XVMC_ERR("Invalid Display, Context or Target!");
        return BadValue;
    }

    ret = (xvmc_driver->render_surface)(display, context, picture_structure,
	    target_surface, past_surface, future_surface, flags,
	    num_macroblocks, first_macroblock, macroblock_array,
	    blocks);

    if (ret) {
	XVMC_ERR("render surface fail\n");
	return BadAccess;
    }
#if 0
    int i;
    int picture_coding_type = MPEG_I_PICTURE;
    /* correction data buffer */
    char *corrdata_ptr;
    int corrdata_size = 0;

    /* Block Pointer */
    short *block_ptr;
    /* Current Macroblock Pointer */
    XvMCMacroBlock *mb;

    i915XvMCSurface *privTarget = NULL;
    i915XvMCSurface *privFuture = NULL;
    i915XvMCSurface *privPast = NULL;
    i915XvMCContext *pI915XvMC = NULL;

    /* Check Parameters for validity */
    if (!display || !context || !target_surface) {
        XVMC_ERR("Invalid Display, Context or Target!");
        return BadValue;
    }

    if (!num_macroblocks)
        return Success;

    if (!macroblock_array || !blocks) {
        XVMC_ERR("Invalid block data!");
        return BadValue;
    }

    if (macroblock_array->num_blocks < (num_macroblocks + first_macroblock)) {
        XVMC_ERR("Too many macroblocks requested for MB array size.");
        return BadValue;
    }

    if (!(pI915XvMC = context->privData))
        return (error_base + XvMCBadContext);

    if (!(privTarget = target_surface->privData))
        return (error_base + XvMCBadSurface);

    /* Test For YV12 Surface */
    if (context->surface_type_id != FOURCC_YV12) {
        XVMC_ERR("HWMC only possible on YV12 Surfaces.");
        return BadValue;
    }

    /* P Frame Test */
    if (!past_surface) {
        /* Just to avoid some ifs later. */
        privPast = privTarget;
    } else {
        if (!(privPast = past_surface->privData)) {
            XVMC_ERR("Invalid Past Surface!");
            return (error_base + XvMCBadSurface);
        }
        
        picture_coding_type = MPEG_P_PICTURE;
    }

    /* B Frame Test */
    if (!future_surface) {
        privFuture = privPast; // privTarget;
    } else {
        if (!past_surface) {
            XVMC_ERR("No Past Surface!");
            return BadValue;
        }

        if (!(privFuture = future_surface->privData)) {
            XVMC_ERR("Invalid Future Surface!");
            return (error_base + XvMCBadSurface);
        }

        picture_coding_type = MPEG_B_PICTURE;
    }

    LOCK_HARDWARE(pI915XvMC);
    corrdata_ptr = pI915XvMC->corrdata.map;
    corrdata_size = 0;

    for (i = first_macroblock; i < (num_macroblocks + first_macroblock); i++) {
        int bspm = 0;
        mb = &macroblock_array->macro_blocks[i];
        block_ptr = &(blocks->blocks[mb->index << 6]);

        /* Lockup can happen if the coordinates are too far out of range */
        if (mb->x > (target_surface->width >> 4)) {
            mb->x = 0;
            XVMC_INFO("reset x");
        }

        if (mb->y > (target_surface->height >> 4)) {
            mb->y = 0;
            XVMC_INFO("reset y");
        }

        /* Catch no pattern case */
        if (!(mb->macroblock_type & XVMC_MB_TYPE_PATTERN) &&
            !(mb->macroblock_type & XVMC_MB_TYPE_INTRA) &&
            mb->coded_block_pattern) {
            mb->coded_block_pattern = 0;
            XVMC_INFO("no coded blocks present!");
        }
        
        bspm = mb_bytes[mb->coded_block_pattern];

        if (!bspm)
            continue;

        corrdata_size += bspm;

        if (corrdata_size > pI915XvMC->corrdata.size) {
            XVMC_ERR("correction data buffer overflow.");
            break;
        }
        memcpy(corrdata_ptr, block_ptr, bspm);
        corrdata_ptr += bspm;
    } 

    i915_flush(pI915XvMC, 1, 0);
    // i915_mc_invalidate_subcontext_buffers(context, BLOCK_SIS | BLOCK_DIS | BLOCK_SSB 
    // | BLOCK_MSB | BLOCK_PSP | BLOCK_PSC);

    i915_mc_sampler_state_buffer(context);
    i915_mc_pixel_shader_program_buffer(context);
    i915_mc_pixel_shader_constants_buffer(context);
    i915_mc_one_time_state_initialization(context);

    i915_mc_static_indirect_state_buffer(context, target_surface, 
                                         picture_structure, flags,
                                         picture_coding_type);
    i915_mc_map_state_buffer(context, privTarget, privPast, privFuture);
    i915_mc_load_sis_msb_buffers(context);
    i915_mc_mpeg_set_origin(context, &macroblock_array->macro_blocks[first_macroblock]);

    for (i = first_macroblock; i < (num_macroblocks + first_macroblock); i++) {
        mb = &macroblock_array->macro_blocks[i];

        /* Intra Blocks */
        if (mb->macroblock_type & XVMC_MB_TYPE_INTRA) {
            i915_mc_mpeg_macroblock_ipicture(context, mb);
        } else if ((picture_structure & XVMC_FRAME_PICTURE) == XVMC_FRAME_PICTURE) { /* Frame Picture */
            switch (mb->motion_type & 3) {
            case XVMC_PREDICTION_FIELD: /* Field Based */
                i915_mc_mpeg_macroblock_2fbmv(context, mb, picture_structure);
                break;

            case XVMC_PREDICTION_FRAME: /* Frame Based */
                i915_mc_mpeg_macroblock_1fbmv(context, mb);
                break;

            case XVMC_PREDICTION_DUAL_PRIME:    /* Dual Prime */
                i915_mc_mpeg_macroblock_2fbmv(context, mb, picture_structure);
                break;

            default:    /* No Motion Type */
                renderError();
                break;
            }   
        } else {        /* Frame Picture */
            switch (mb->motion_type & 3) {
            case XVMC_PREDICTION_FIELD: /* Field Based */
                i915_mc_mpeg_macroblock_1fbmv(context, mb);
                break;

            case XVMC_PREDICTION_16x8:  /* 16x8 MC */
                i915_mc_mpeg_macroblock_2fbmv(context, mb, picture_structure);
                break;
                
            case XVMC_PREDICTION_DUAL_PRIME:    /* Dual Prime */
                i915_mc_mpeg_macroblock_1fbmv(context, mb);
                break;

            default:    /* No Motion Type */
                renderError();
                break;
            }
        }       /* Field Picture */
    }

    intelFlushBatch(pI915XvMC, TRUE);
    pI915XvMC->last_render = pI915XvMC->alloc.irq_emitted;
    privTarget->last_render = pI915XvMC->last_render;

    UNLOCK_HARDWARE(pI915XvMC);
#endif
    return Success;
}

/***************************************************************************
// Function: XvMCPutSurface
// Description:
// Arguments:
//  display: Connection to X server
//  surface: Surface to be displayed
//  draw: X Drawable on which to display the surface
//  srcx: X coordinate of the top left corner of the region to be
//          displayed within the surface.
//  srcy: Y coordinate of the top left corner of the region to be
//          displayed within the surface.
//  srcw: Width of the region to be displayed.
//  srch: Height of the region to be displayed.
//  destx: X cordinate of the top left corner of the destination region
//         in the drawable coordinates.
//  desty: Y cordinate of the top left corner of the destination region
//         in the drawable coordinates.
//  destw: Width of the destination region.
//  desth: Height of the destination region.
//  flags: One or more of the following.
//     XVMC_TOP_FIELD - Display only the Top field of the surface.
//     XVMC_BOTTOM_FIELD - Display only the Bottom Field of the surface.
//     XVMC_FRAME_PICTURE - Display both fields or frame.
//
// Info: Portions of this function derived from i915_video.c (XFree86)
//
//   This function is organized so that we wait as long as possible before
//   touching the overlay registers. Since we don't know that the last
//   flip has happened yet we want to give the overlay as long as
//   possible to catch up before we have to check on its progress. This
//   makes it unlikely that we have to wait on the last flip.
***************************************************************************/
Status XvMCPutSurface(Display *display,XvMCSurface *surface,
                      Drawable draw, short srcx, short srcy,
                      unsigned short srcw, unsigned short srch,
                      short destx, short desty,
                      unsigned short destw, unsigned short desth,
                      int flags)
{
    int ret = -1;

    if (!display || !surface)
        return BadValue;

    ret = (xvmc_driver->put_surface)(display, surface, draw, srcx, srcy,
	    srcw, srch, destx, desty, destw, desth, flags);
    if (ret) {
	XVMC_ERR("put surface fail\n");
	return BadAccess;
    }

#if 0
    i915XvMCContext *pI915XvMC;
    i915XvMCSurface *pI915Surface;
    i915XvMCSubpicture *pI915SubPic;
    I915XvMCCommandBuffer buf;

    // drawableInfo *drawInfo;
    Status ret;

    if (!display || !surface)
        return BadValue;

    if (!(pI915Surface = surface->privData))
        return (error_base + XvMCBadSurface);

    if (!(pI915XvMC = pI915Surface->privContext))
        return (error_base + XvMCBadSurface);

    PPTHREAD_MUTEX_LOCK(pI915XvMC);
    /*
    if (getDRIDrawableInfoLocked(pI915XvMC->drawHash, display,
                                 pI915XvMC->screen, draw, 0, pI915XvMC->fd, pI915XvMC->hHWContext,
                                 pI915XvMC->sarea_address, FALSE, &drawInfo, sizeof(*drawInfo))) {
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
        return BadAccess;
    }
    */
    if (!pI915XvMC->haveXv) {
        pI915XvMC->xvImage =
            XvCreateImage(display, pI915XvMC->port, FOURCC_XVMC,
                          (char *)&buf, pI915Surface->width, pI915Surface->height);
        pI915XvMC->gc = XCreateGC(display, draw, 0, 0);
        pI915XvMC->haveXv = 1;
    }

    pI915XvMC->draw = draw;
    pI915XvMC->xvImage->data = (char *)&buf;

    buf.command = INTEL_XVMC_COMMAND_DISPLAY;
    buf.ctxNo = pI915XvMC->ctxno;
    buf.srfNo = pI915Surface->srfNo;
    pI915SubPic = pI915Surface->privSubPic;
    buf.subPicNo = (!pI915SubPic ? 0 : pI915SubPic->srfNo);
    buf.real_id = FOURCC_YV12;

    XLockDisplay(display);

    if ((ret = XvPutImage(display, pI915XvMC->port, draw, pI915XvMC->gc,
                          pI915XvMC->xvImage, srcx, srcy, srcw, srch,
                          destx, desty, destw, desth))) {
        XUnlockDisplay(display);
        PPTHREAD_MUTEX_UNLOCK(pI915XvMC);

        return ret;
    }

    XSync(display, 0);
    XUnlockDisplay(display);
    PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
#endif

    return Success;
}

/***************************************************************************
// Function: XvMCSyncSurface
// Arguments:
//   display - Connection to the X server
//   surface - The surface to synchronize
// Info:
// Returns: Status
***************************************************************************/
Status XvMCSyncSurface(Display *display, XvMCSurface *surface) 
{
    Status ret;
    int stat = 0;

    do {
        ret = XvMCGetSurfaceStatus(display, surface, &stat);
    } while (!ret && (stat & XVMC_RENDERING));

    return ret;
}

/***************************************************************************
// Function: XvMCFlushSurface
// Description:
//   This function commits pending rendering requests to ensure that they
//   wll be completed in a finite amount of time.
// Arguments:
//   display - Connection to X server
//   surface - Surface to flush
// Returns: Status
***************************************************************************/
Status XvMCFlushSurface(Display * display, XvMCSurface *surface) 
{
    return Success;
}

/***************************************************************************
// Function: XvMCGetSurfaceStatus
// Description:
// Arguments:
//  display: connection to X server
//  surface: The surface to query
//  stat: One of the Following
//    XVMC_RENDERING - The last XvMCRenderSurface command has not
//                     completed.
//    XVMC_DISPLAYING - The surface is currently being displayed or a
//                     display is pending.
***************************************************************************/
Status XvMCGetSurfaceStatus(Display *display, XvMCSurface *surface, int *stat) 
{
    int ret = -1;

    if (!display || !surface || !stat)
        return BadValue;

    ret = (xvmc_driver->get_surface_status)(display, surface, stat);
    if (ret) {
	XVMC_ERR("get surface status fail\n");
	return BadAccess;
    }

#if 0
    i915XvMCSurface *pI915Surface;
    i915XvMCContext *pI915XvMC;

    if (!display || !surface || !stat)
        return BadValue;
    
    *stat = 0;

    if (!(pI915Surface = surface->privData))
        return (error_base + XvMCBadSurface);

    if (!(pI915XvMC = pI915Surface->privContext))
        return (error_base + XvMCBadSurface);

    // LOCK_HARDWARE(pI915XvMC);
    PPTHREAD_MUTEX_LOCK(pI915XvMC);
    if (pI915Surface->last_flip) {
        /* This can not happen */
        if (pI915XvMC->last_flip < pI915Surface->last_flip) {
            XVMC_ERR("Context last flip is less than surface last flip.");
            PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
            return BadValue;
        }

        /*
          If the context has 2 or more flips after this surface it
          cannot be displaying. Don't bother to check.
        */
        if (!(pI915XvMC->last_flip > (pI915Surface->last_flip + 1))) {
            /*
              If this surface was the last flipped it is either displaying
              or about to be so don't bother checking.
            */
            if (pI915XvMC->last_flip == pI915Surface->last_flip) {
                *stat |= XVMC_DISPLAYING;
            }
        }
    }

    if (pI915Surface->last_render &&
        (pI915Surface->last_render > pI915XvMC->sarea->last_dispatch)) {
        *stat |= XVMC_RENDERING;
    }

    // UNLOCK_HARDWARE(pI915XvMC);
    PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
#endif

    return Success;
}

/***************************************************************************
// 
//  Surface manipulation functions
//
***************************************************************************/

/***************************************************************************
// Function: XvMCHideSurface
// Description: Stops the display of a surface.
// Arguments:
//   display - Connection to the X server.
//   surface - surface to be hidden.
//
// Returns: Status
***************************************************************************/
//XXX this seems broken now
Status XvMCHideSurface(Display *display, XvMCSurface *surface) 
{
//    i915XvMCSurface *pI915Surface;
//    i915XvMCContext *pI915XvMC;
    int stat = 0, ret;

    if (!display || !surface)
        return BadValue;

#if 0
    if (!(pI915Surface = surface->privData))
        return (error_base + XvMCBadSurface);

    /* Get the associated context pointer */
    if (!(pI915XvMC = pI915Surface->privContext))
        return (error_base + XvMCBadSurface);
#endif

    XvMCSyncSurface(display, surface);

    /*
      Get the status of the surface, if it is not currently displayed
      we don't need to worry about it.
    */
    if ((ret = XvMCGetSurfaceStatus(display, surface, &stat)) != Success)
        return ret;

    if (!(stat & XVMC_DISPLAYING))
        return Success;

    /* FIXME: */
    return Success;
}

/***************************************************************************
//
// Functions that deal with subpictures
//
***************************************************************************/



/***************************************************************************
// Function: XvMCCreateSubpicture
// Description: This creates a subpicture by filling out the XvMCSubpicture
//              structure passed to it and returning Success.
// Arguments:
//   display - Connection to the X server.
//   context - The context to create the subpicture for.
//   subpicture - Pre-allocated XvMCSubpicture structure to be filled in.
//   width - of subpicture
//   height - of subpicture
//   xvimage_id - The id describing the XvImage format.
//
// Returns: Status
***************************************************************************/
Status XvMCCreateSubpicture(Display *display, XvMCContext *context,
                            XvMCSubpicture *subpicture,
                            unsigned short width, unsigned short height,
                            int xvimage_id) 
{
    return BadValue;
}

/***************************************************************************
// Function: XvMCClearSubpicture
// Description: Clear the area of the given subpicture to "color".
//              structure passed to it and returning Success.
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to clear.
//   x, y, width, height - rectangle in the subpicture to clear.
//   color - The data to file the rectangle with.
//
// Returns: Status
***************************************************************************/
Status XvMCClearSubpicture(Display *display, XvMCSubpicture *subpicture,
                           short x, short y,
                           unsigned short width, unsigned short height,
                           unsigned int color) 
{
    return BadValue;
}

/***************************************************************************
// Function: XvMCCompositeSubpicture
// Description: Composite the XvImae on the subpicture. This composit uses
//              non-premultiplied alpha. Destination alpha is utilized
//              except for with indexed subpictures. Indexed subpictures
//              use a simple "replace".
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to clear.
//   image - the XvImage to be used as the source of the composite.
//   srcx, srcy, width, height - The rectangle from the image to be used.
//   dstx, dsty - location in the subpicture to composite the source.
//
// Returns: Status
***************************************************************************/
Status XvMCCompositeSubpicture(Display *display, XvMCSubpicture *subpicture,
                               XvImage *image,
                               short srcx, short srcy,
                               unsigned short width, unsigned short height,
                               short dstx, short dsty) 
{
    return BadValue;
}


/***************************************************************************
// Function: XvMCDestroySubpicture
// Description: Destroys the specified subpicture.
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpicture to be destroyed.
//
// Returns: Status
***************************************************************************/
Status XvMCDestroySubpicture(Display *display, XvMCSubpicture *subpicture) 
{
    return BadValue;
}


/***************************************************************************
// Function: XvMCSetSubpicturePalette
// Description: Set the subpictures palette
// Arguments:
//   display - Connection to the X server.
//   subpicture - Subpiture to set palette for.
//   palette - A pointer to an array holding the palette data. The array
//     is num_palette_entries * entry_bytes in size.
// Returns: Status
***************************************************************************/

Status XvMCSetSubpicturePalette(Display *display, XvMCSubpicture *subpicture,
                                unsigned char *palette) 
{
    return BadValue;
}

/***************************************************************************
// Function: XvMCBlendSubpicture
// Description: 
//    The behavior of this function is different depending on whether
//    or not the XVMC_BACKEND_SUBPICTURE flag is set in the XvMCSurfaceInfo.
//    i915 only support frontend behavior.
//  
//    XVMC_BACKEND_SUBPICTURE not set ("frontend" behavior):
//   
//    XvMCBlendSubpicture is a no-op in this case.
//   
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to be blended into the video.
//   target_surface - The surface to be displayed with the blended subpic.
//   source_surface - Source surface prior to blending.
//   subx, suby, subw, subh - The rectangle from the subpicture to use.
//   surfx, surfy, surfw, surfh - The rectangle in the surface to blend
//      blend the subpicture rectangle into. Scaling can ocure if 
//      XVMC_SUBPICTURE_INDEPENDENT_SCALING is set.
//
// Returns: Status
***************************************************************************/
Status XvMCBlendSubpicture(Display *display, XvMCSurface *target_surface,
                           XvMCSubpicture *subpicture,
                           short subx, short suby,
                           unsigned short subw, unsigned short subh,
                           short surfx, short surfy,
                           unsigned short surfw, unsigned short surfh) 
{
    return BadValue;
}

/***************************************************************************
// Function: XvMCBlendSubpicture2
// Description: 
//    The behavior of this function is different depending on whether
//    or not the XVMC_BACKEND_SUBPICTURE flag is set in the XvMCSurfaceInfo.
//    i915 only supports frontend blending.
//  
//    XVMC_BACKEND_SUBPICTURE not set ("frontend" behavior):
//   
//    XvMCBlendSubpicture2 blends the source_surface and subpicture and
//    puts it in the target_surface.  This does not effect the status of
//    the source surface but will cause the target_surface to query
//    XVMC_RENDERING until the blend is completed.
//   
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to be blended into the video.
//   target_surface - The surface to be displayed with the blended subpic.
//   source_surface - Source surface prior to blending.
//   subx, suby, subw, subh - The rectangle from the subpicture to use.
//   surfx, surfy, surfw, surfh - The rectangle in the surface to blend
//      blend the subpicture rectangle into. Scaling can ocure if 
//      XVMC_SUBPICTURE_INDEPENDENT_SCALING is set.
//
// Returns: Status
***************************************************************************/
Status XvMCBlendSubpicture2(Display *display, 
                            XvMCSurface *source_surface,
                            XvMCSurface *target_surface,
                            XvMCSubpicture *subpicture,
                            short subx, short suby,
                            unsigned short subw, unsigned short subh,
                            short surfx, short surfy,
                            unsigned short surfw, unsigned short surfh)
{
    return BadValue;
}

/***************************************************************************
// Function: XvMCSyncSubpicture
// Description: This function blocks until all composite/clear requests on
//              the subpicture have been complete.
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture to synchronize
//
// Returns: Status
***************************************************************************/
Status XvMCSyncSubpicture(Display *display, XvMCSubpicture *subpicture) 
{
    return BadValue;
#if 0
    Status ret;
    int stat = 0;

    if (!display || !subpicture)

    do {
        ret = XvMCGetSubpictureStatus(display, subpicture, &stat);
    } while(!ret && (stat & XVMC_RENDERING));

    return ret;
#endif
}

/***************************************************************************
// Function: XvMCFlushSubpicture
// Description: This function commits pending composite/clear requests to
//              ensure that they will be completed in a finite amount of
//              time.
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture whos compsiting should be flushed
//
// Returns: Status
***************************************************************************/
Status XvMCFlushSubpicture(Display *display, XvMCSubpicture *subpicture) 
{
    return BadValue;
#if 0
    i915XvMCSubpicture *pI915Subpicture;

    if (!display || !subpicture)
        return BadValue;

    if (!(pI915Subpicture = subpicture->privData))
        return (error_base + XvMCBadSubpicture);

    return Success;
#endif
}

/***************************************************************************
// Function: XvMCGetSubpictureStatus
// Description: This function gets the current status of a subpicture
//
// Arguments:
//   display - Connection to the X server.
//   subpicture - The subpicture whos status is being queried
//   stat - The status of the subpicture. It can be any of the following
//          OR'd together:
//          XVMC_RENDERING  - Last composite or clear request not completed
//          XVMC_DISPLAYING - Suppicture currently being displayed.
//
// Returns: Status
***************************************************************************/
Status XvMCGetSubpictureStatus(Display *display, XvMCSubpicture *subpicture,
                               int *stat) 
{
    return BadValue;
#if 0
    i915XvMCSubpicture *pI915Subpicture;
    i915XvMCContext *pI915XvMC;

    if (!display || !subpicture || stat)
        return BadValue;

    *stat = 0;

    if (!(pI915Subpicture = subpicture->privData))
        return (error_base + XvMCBadSubpicture);

    if (!(pI915XvMC = pI915Subpicture->privContext))
        return (error_base + XvMCBadSubpicture);

    // LOCK_HARDWARE(pI915XvMC);
    PPTHREAD_MUTEX_LOCK(pI915XvMC);
    /* FIXME: */
    if (pI915Subpicture->last_render &&
        (pI915Subpicture->last_render > pI915XvMC->sarea->last_dispatch)) {
        *stat |= XVMC_RENDERING;
    }

    // UNLOCK_HARDWARE(pI915XvMC);
    PPTHREAD_MUTEX_UNLOCK(pI915XvMC);
    return Success;
#endif
}

/***************************************************************************
// Function: XvMCQueryAttributes
// Description: An array of XvAttributes of size "number" is returned by
//   this function. If there are no attributes, NULL is returned and number
//   is set to 0. The array may be freed with xfree().
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   number - The returned number of recognized atoms
//
// Returns:
//  An array of XvAttributes.
***************************************************************************/
XvAttribute *XvMCQueryAttributes(Display *display, XvMCContext *context,
                                 int *number) 
{
    /* now XvMC has no extra attribs than Xv */
    *number = 0;
    return NULL;
}

/***************************************************************************
// Function: XvMCSetAttribute
// Description: This function sets a context-specific attribute.
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   attribute - The X atom of the attribute to be changed.
//   value - The new value for the attribute.
//
// Returns:
//  Status
***************************************************************************/
Status XvMCSetAttribute(Display *display, XvMCContext *context,
                        Atom attribute, int value)
{
    return Success;
}

/***************************************************************************
// Function: XvMCGetAttribute
// Description: This function queries a context-specific attribute and
//   returns the value.
//
// Arguments:
//   display - Connection to the X server.
//   context - The context whos attributes we are querying.
//   attribute - The X atom of the attribute to be queried
//   value - The returned attribute value
//
// Returns:
//  Status
// Notes:
***************************************************************************/
Status XvMCGetAttribute(Display *display, XvMCContext *context,
                        Atom attribute, int *value) 
{
    return Success;
}
