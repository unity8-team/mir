#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "i830.h"
#include "i830_dri.h"
#include "i830_video.h"
#include "xf86xv.h"
#include "xf86xvmc.h"
#include <X11/extensions/Xv.h>
#include <X11/extensions/XvMC.h>
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#if defined(X_NEED_XVPRIV_H) || defined (_XF86_FOURCC_H_)
#include "xf86xvpriv.h"
#endif

#include "i915_hwmc.h"

#define MAKE_ATOM(a) MakeAtom(a, strlen(a), TRUE)

/*
 * List Attributes for the XvMC extension to handle:
 * As long as the attribute is supported by the Xv adaptor, it needs only to
 * be added here to be supported also by XvMC.
 * Currently, only colorkey seems to be supported by Xv for Putimage.
 */
static char *attrXvMC[I915_NUM_XVMC_ATTRIBUTES] = { 
    "XV_BRIGHTNESS",
    "XV_CONTRAST",
};
static Atom attrAtoms[I915_NUM_XVMC_ATTRIBUTES];

typedef struct
{
    unsigned ctxDisplaying;
    int xvmc_port;
    I915XvMCAttrHolder xvAttr;
    int newAttribute;

    SetPortAttributeFuncPtr SetPortAttribute;
    GetPortAttributeFuncPtr GetPortAttribute;
    PutImageFuncPtr PutImage;
} I915XvMCXVPriv;

#define I915_XVMC_MAX_BUFFERS 2
#define I915_XVMC_MAX_CONTEXTS 4
#define I915_XVMC_MAX_SURFACES 20

typedef struct _I915XvMCSurfacePriv
{
    i830_memory *surface;
    unsigned long offsets[I915_XVMC_MAX_BUFFERS];
} I915XvMCSurfacePriv;

typedef struct _I915XvMCContextPriv
{
    i830_memory *mcStaticIndirectState;
    drm_handle_t sis_handle;
    i830_memory *mcMapState;
    drm_handle_t msb_handle;
    i830_memory *mcSamplerState;
    drm_handle_t ssb_handle;
    i830_memory *mcPixelShaderProgram;
    drm_handle_t psp_handle;
    i830_memory *mcPixelShaderConstants;
    drm_handle_t psc_handle;
    i830_memory *mcCorrdata;
    drm_handle_t corrdata_handle;
} I915XvMCContextPriv;

typedef struct _I915XvMC 
{
    XID contexts[I915_XVMC_MAX_CONTEXTS];
    XID surfaces[I915_XVMC_MAX_SURFACES];
    I915XvMCSurfacePriv *sfprivs[I915_XVMC_MAX_SURFACES];
    I915XvMCContextPriv *ctxprivs[I915_XVMC_MAX_CONTEXTS];
    int ncontexts,nsurfaces;
} I915XvMC, *I915XvMCPtr;

#define ARRARY_SIZE(a) (sizeof(a) / sizeof(a[0]))
static int I915XvMCCreateContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext,
                                  int *num_priv, long **priv );
static void I915XvMCDestroyContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext);

static int I915XvMCCreateSurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf,
                                  int *num_priv, long **priv );
static void I915XvMCDestroySurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf);

static int I915XvMCCreateSubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSurf,
                                     int *num_priv, long **priv );
static void I915XvMCDestroySubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSurf);

static int yv12_subpicture_index_list[2] = 
{
    FOURCC_IA44,
    FOURCC_AI44
};

static XF86MCImageIDList yv12_subpicture_list =
{
    ARRARY_SIZE(yv12_subpicture_index_list),
    yv12_subpicture_index_list
};
 
static XF86MCSurfaceInfoRec i915_YV12_mpg2_surface =
{
    FOURCC_YV12,  
    XVMC_CHROMA_FORMAT_420,
    0,
    720,
    576,
    720,
    576,
    XVMC_MPEG_2,
    XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING |
    XVMC_INTRA_UNSIGNED,
    &yv12_subpicture_list
};

static XF86MCSurfaceInfoRec i915_YV12_mpg1_surface =
{
    FOURCC_YV12,  
    XVMC_CHROMA_FORMAT_420,
    0,
    720,
    576,
    720,
    576,
    XVMC_MPEG_1,
    XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING |
    XVMC_INTRA_UNSIGNED,
    &yv12_subpicture_list
};

static XF86MCSurfaceInfoPtr ppSI[2] = 
{
    (XF86MCSurfaceInfoPtr)&i915_YV12_mpg2_surface,
    (XF86MCSurfaceInfoPtr)&i915_YV12_mpg1_surface
};

/* List of subpicture types that we support */
static XF86ImageRec ia44_subpicture = XVIMAGE_IA44;
static XF86ImageRec ai44_subpicture = XVIMAGE_AI44;

static XF86ImagePtr i915_subpicture_list[2] =
{
    (XF86ImagePtr)&ia44_subpicture,
    (XF86ImagePtr)&ai44_subpicture
};

/* Fill in the device dependent adaptor record. 
 * This is named "Intel(R) Textured Video" because this code falls under the
 * XV extenstion, the name must match or it won't be used.
 *
 * Surface and Subpicture - see above
 * Function pointers to functions below
 */
static XF86MCAdaptorRec pAdapt = 
{
    "Intel(R) Textured Video",		                        /* name */
    ARRARY_SIZE(ppSI),                                          /* num_surfaces */
    ppSI,				                        /* surfaces */
    ARRARY_SIZE(i915_subpicture_list),                          /* num_subpictures */
    i915_subpicture_list,		                        /* subpictures */
    (xf86XvMCCreateContextProcPtr)I915XvMCCreateContext,
    (xf86XvMCDestroyContextProcPtr)I915XvMCDestroyContext,
    (xf86XvMCCreateSurfaceProcPtr)I915XvMCCreateSurface,
    (xf86XvMCDestroySurfaceProcPtr)I915XvMCDestroySurface,
    (xf86XvMCCreateSubpictureProcPtr)I915XvMCCreateSubpicture,
    (xf86XvMCDestroySubpictureProcPtr)I915XvMCDestroySubpicture
};

static XF86MCAdaptorPtr ppAdapt[1] = 
{
    (XF86MCAdaptorPtr)&pAdapt
};

static unsigned int stride(int w)
{
    return (w + 31) & ~31;
}

static unsigned long size_yuv420(int w, int h)
{
    unsigned cpp = 4;
    unsigned yPitch = stride(w) * cpp;
    unsigned uvPitch = stride(w / 2) * cpp;

    return h * (yPitch + uvPitch);
}

static unsigned long size_xx44(int w, int h)
{
    return h * stride(w);
}

/*
 * Init and clean up the screen private parts of XvMC.
 */
static void initI915XvMC(I915XvMCPtr xvmc)
{
    unsigned int i;

    for (i = 0; i < I915_XVMC_MAX_CONTEXTS; i++) {
        xvmc->contexts[i] = 0;
        xvmc->ctxprivs[i] = NULL;
    }

    for (i = 0; i < I915_XVMC_MAX_SURFACES; i++) {
        xvmc->surfaces[i] = 0;
        xvmc->sfprivs[i] = NULL;
    }
}

static void cleanupI915XvMC(I915XvMCPtr xvmc, XF86VideoAdaptorPtr * XvAdaptors, int XvAdaptorCount)
{
    unsigned int i;

    for (i = 0; i < I915_XVMC_MAX_CONTEXTS; i++) {
        xvmc->contexts[i] = 0;

        if (xvmc->ctxprivs[i]) {
            xfree(xvmc->ctxprivs[i]);
            xvmc->ctxprivs[i] = NULL;
        }
    }

    for (i = 0; i < I915_XVMC_MAX_SURFACES; i++) {
        xvmc->surfaces[i] = 0;
        
        if (xvmc->sfprivs[i]) {
            xfree(xvmc->sfprivs[i]);
            xvmc->sfprivs[i] = NULL;
        }
    }
}

static Bool i915_map_xvmc_buffers(ScrnInfoPtr pScrn, I915XvMCContextPriv *ctxpriv)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (drmAddMap(pI830->drmSubFD,
                  (drm_handle_t)(ctxpriv->mcStaticIndirectState->offset + pI830->LinearAddr),
                  ctxpriv->mcStaticIndirectState->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->sis_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddMap(sis_handle) failed!\n");
        return FALSE;
    }

    if (drmAddMap(pI830->drmSubFD,
                  (drm_handle_t)(ctxpriv->mcMapState->offset + pI830->LinearAddr),
                  ctxpriv->mcMapState->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->msb_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddMap(msb_handle) failed!\n");
        return FALSE;
    }

    if (drmAddMap(pI830->drmSubFD, 
                  (drm_handle_t)(ctxpriv->mcSamplerState->offset + pI830->LinearAddr),
                  ctxpriv->mcSamplerState->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->ssb_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddress(ssb_handle) failed!\n");
        return FALSE;
    }

    if (drmAddMap(pI830->drmSubFD, 
                  (drm_handle_t)(ctxpriv->mcPixelShaderProgram->offset + pI830->LinearAddr),
                  ctxpriv->mcPixelShaderProgram->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->psp_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddress(psp_handle) failed!\n");
        return FALSE;
    }

    if (drmAddMap(pI830->drmSubFD, 
                  (drm_handle_t)(ctxpriv->mcPixelShaderConstants->offset + pI830->LinearAddr),
                  ctxpriv->mcPixelShaderConstants->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->psc_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddress(psc_handle) failed!\n");
        return FALSE;
    }

    if (drmAddMap(pI830->drmSubFD,
                  (drm_handle_t)(ctxpriv->mcCorrdata->offset + pI830->LinearAddr),
                  ctxpriv->mcCorrdata->size, DRM_AGP, 0,
                  (drmAddress)&ctxpriv->corrdata_handle) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[drm] drmAddMap(corrdata_handle) failed!\n");
        return FALSE;
    }

    return TRUE;
}

static void i915_unmap_xvmc_buffers(ScrnInfoPtr pScrn, I915XvMCContextPriv *ctxpriv)
{
    I830Ptr pI830 = I830PTR(pScrn);
    
    if (ctxpriv->sis_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->sis_handle);
        ctxpriv->sis_handle = 0;
    }

    if (ctxpriv->msb_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->msb_handle);
        ctxpriv->msb_handle = 0;
    }

    if (ctxpriv->ssb_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->ssb_handle);
        ctxpriv->ssb_handle = 0;
    }

    if (ctxpriv->psp_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->psp_handle);
        ctxpriv->psp_handle = 0;
    }

    if (ctxpriv->psc_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->psc_handle);
        ctxpriv->psc_handle = 0;
    }

    if (ctxpriv->corrdata_handle) {
        drmRmMap(pI830->drmSubFD, ctxpriv->corrdata_handle);
        ctxpriv->corrdata_handle = 0;
    }
}

static Bool i915_allocate_xvmc_buffers(ScrnInfoPtr pScrn, I915XvMCContextPriv *ctxpriv)
{
    if (!i830_allocate_xvmc_buffer(pScrn, "Static Indirect State", 
                                   &(ctxpriv->mcStaticIndirectState), 512)) {
        return FALSE;
    }

    if (!i830_allocate_xvmc_buffer(pScrn, "Map State", 
                                   &(ctxpriv->mcMapState), 512)) {
        return FALSE;
    }

    if (!i830_allocate_xvmc_buffer(pScrn, "Sampler State",
                                   &(ctxpriv->mcSamplerState), 512)) {
        return FALSE;
    }

    if (!i830_allocate_xvmc_buffer(pScrn, "Pixel Shader Program",
                                   &(ctxpriv->mcPixelShaderProgram), 512)) {
        return FALSE;
    }

    if (!i830_allocate_xvmc_buffer(pScrn, "Pixel Shader Constants",
                                   &(ctxpriv->mcPixelShaderConstants), 128)) {
        return FALSE;
    }

    if (!i830_allocate_xvmc_buffer(pScrn, "Correction Data Buffer", 
                                   &(ctxpriv->mcCorrdata), 2 * 1024 * 1024)) {
        return FALSE;
    }

    return TRUE;
}

static void i915_free_xvmc_buffers(ScrnInfoPtr pScrn, I915XvMCContextPriv *ctxpriv)
{
    if (ctxpriv->mcStaticIndirectState) {
        i830_free_memory(pScrn, ctxpriv->mcStaticIndirectState);
        ctxpriv->mcStaticIndirectState = NULL;
    }

    if (ctxpriv->mcMapState) {
        i830_free_memory(pScrn, ctxpriv->mcMapState);
        ctxpriv->mcMapState = NULL;
    }

    if (ctxpriv->mcSamplerState) {
        i830_free_memory(pScrn, ctxpriv->mcSamplerState);
        ctxpriv->mcSamplerState = NULL;
    }

    if (ctxpriv->mcPixelShaderProgram) {
        i830_free_memory(pScrn, ctxpriv->mcPixelShaderProgram);
        ctxpriv->mcPixelShaderProgram = NULL;
    }

    if (ctxpriv->mcPixelShaderConstants) {
        i830_free_memory(pScrn, ctxpriv->mcPixelShaderConstants);
        ctxpriv->mcPixelShaderConstants = NULL;
    }

    if (ctxpriv->mcCorrdata) {
        i830_free_memory(pScrn, ctxpriv->mcCorrdata);
        ctxpriv->mcCorrdata = NULL;
    }
}

/**************************************************************************
 *
 *  I915XvMCCreateContext
 *
 *  Some info about the private data:
 *
 *  Set *num_priv to the number of 32bit words that make up the size of
 *  of the data that priv will point to.
 *
 *  *priv = (long *) xcalloc (elements, sizeof(element))
 *  *num_priv = (elements * sizeof(element)) >> 2;
 *
 **************************************************************************/

static int I915XvMCCreateContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext,
                                  int *num_priv, long **priv )
{
    I830Ptr pI830 = I830PTR(pScrn);
    DRIInfoPtr pDRIInfo = pI830->pDRIInfo;
    I915XvMCCreateContextRec *contextRec = NULL;
    I915XvMCPtr pXvMC = pI830->xvmc;
    I915XvMCContextPriv *ctxpriv = NULL;
    XvPortRecPrivatePtr portPriv = (XvPortRecPrivatePtr)pContext->port_priv;
    I830PortPrivPtr pPriv = (I830PortPrivPtr)portPriv->DevPriv.ptr;
    I915XvMCXVPriv *vx = (I915XvMCXVPriv *)pPriv->xvmc_priv;

    int i;

    *priv = NULL;
    *num_priv = 0;

    if (!pI830->directRenderingEnabled) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateContext: Cannot use XvMC without DRI!\n");
        return BadAlloc;
    }

    if (pXvMC->ncontexts >= I915_XVMC_MAX_CONTEXTS) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateContext: Out of contexts.\n");
        return BadAlloc;
    }

    *priv = xcalloc(1, sizeof(I915XvMCCreateContextRec));
    contextRec = (I915XvMCCreateContextRec *)*priv;

    if (!*priv) {
        *num_priv = 0;
        return BadAlloc;
    }

    *num_priv = sizeof(I915XvMCCreateContextRec) >> 2;

    for (i = 0; i < I915_XVMC_MAX_CONTEXTS; i++) {
        if (!pXvMC->contexts[i])
            break;
    }

    ctxpriv = (I915XvMCContextPriv *)xcalloc(1, sizeof(I915XvMCContextPriv));

    if (!ctxpriv) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateContext: Unable to allocate memory!\n");
        xfree(*priv);
        *priv = NULL;
        *num_priv = 0;
        return BadAlloc;
    }

    if (!i915_allocate_xvmc_buffers(pScrn, ctxpriv)) {
        i915_free_xvmc_buffers(pScrn, ctxpriv);
        xfree(ctxpriv);
        ctxpriv = NULL;
        xfree(*priv);
        *priv = NULL;
        *num_priv = 0;
        return BadAlloc;
    }

    if (!i915_map_xvmc_buffers(pScrn, ctxpriv)) {
        i915_unmap_xvmc_buffers(pScrn, ctxpriv);
        i915_free_xvmc_buffers(pScrn, ctxpriv);
        xfree(ctxpriv);
        ctxpriv = NULL;
        xfree(*priv);
        *priv = NULL;
        *num_priv = 0;
        return BadAlloc;
    }

    contextRec->ctxno = i;
    contextRec->sis.handle = ctxpriv->sis_handle;
    contextRec->sis.offset = ctxpriv->mcStaticIndirectState->offset;
    contextRec->sis.size = ctxpriv->mcStaticIndirectState->size;
    contextRec->msb.handle = ctxpriv->msb_handle;
    contextRec->msb.offset = ctxpriv->mcMapState->offset;
    contextRec->msb.size = ctxpriv->mcMapState->size;
    contextRec->ssb.handle = ctxpriv->ssb_handle;
    contextRec->ssb.offset = ctxpriv->mcSamplerState->offset;
    contextRec->ssb.size = ctxpriv->mcSamplerState->size;
    contextRec->psp.handle = ctxpriv->psp_handle;
    contextRec->psp.offset = ctxpriv->mcPixelShaderProgram->offset;
    contextRec->psp.size = ctxpriv->mcPixelShaderProgram->size;
    contextRec->psc.handle = ctxpriv->psc_handle;
    contextRec->psc.offset = ctxpriv->mcPixelShaderConstants->offset;
    contextRec->psc.size = ctxpriv->mcPixelShaderConstants->size;
    contextRec->corrdata.handle = ctxpriv->corrdata_handle;
    contextRec->corrdata.offset = ctxpriv->mcCorrdata->offset;
    contextRec->corrdata.size = ctxpriv->mcCorrdata->size;
    contextRec->sarea_size = pDRIInfo->SAREASize;
    contextRec->sarea_priv_offset = sizeof(XF86DRISAREARec);
    contextRec->screen = pScrn->pScreen->myNum;
    contextRec->depth = pScrn->bitsPerPixel;
    contextRec->initAttrs = vx->xvAttr;

    pXvMC->ncontexts++;
    pXvMC->contexts[i] = pContext->context_id;
    pXvMC->ctxprivs[i] = ctxpriv;

    return Success;
}

static int I915XvMCCreateSurface(ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf,
                                 int *num_priv, long **priv )
{
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = pI830->xvmc;
    I915XvMCSurfacePriv *sfpriv = NULL;
    XvMCContextPtr ctx = NULL;
    unsigned int sfno, i, nbuffers;
    unsigned long bufsize;

    *priv = NULL;
    *num_priv = 0;

    if (pXvMC->nsurfaces >= I915_XVMC_MAX_SURFACES) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSurface: Too many surfaces !\n");
        return BadAlloc;
    }

    sfpriv = (I915XvMCSurfacePriv *)xcalloc(1, sizeof(I915XvMCSurfacePriv));

    if (!sfpriv) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSurface: Unable to allocate memory!\n");
        return BadAlloc;
    }

    nbuffers = 1;
    *num_priv = nbuffers + 2;
    *priv = (long *)xcalloc(*num_priv, sizeof(unsigned long));
    
    if (!*priv) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSurface: Unable to allocate memory!\n");
        xfree(sfpriv);
        *num_priv = 0;
        return BadAlloc;
    }

    ctx = pSurf->context;
    bufsize = size_yuv420(ctx->width, ctx->height);

    if (!i830_allocate_xvmc_surface(pScrn, &(sfpriv->surface), nbuffers * bufsize)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSurface: Failed to allocate XvMC surface space!\n");
        xfree(sfpriv);
        xfree(*priv);
        *priv = NULL;
        *num_priv = 0;
        return BadAlloc;
    }

    for (sfno = 0; sfno < I915_XVMC_MAX_SURFACES; ++sfno) {
        if (!pXvMC->surfaces[sfno])
            break;
    }

    (*priv)[0] = sfno;
    (*priv)[1] = nbuffers;
    (*priv)[2] = sfpriv->offsets[0] = sfpriv->surface->offset;

    for (i = 1; i < nbuffers; ++i) {
        (*priv)[i + 2] = sfpriv->offsets[i] = sfpriv->offsets[i - 1] + bufsize;
    }

    pXvMC->surfaces[sfno] = pSurf->surface_id;
    pXvMC->sfprivs[sfno]= sfpriv;
    pXvMC->nsurfaces++;

    return Success;
}

static int I915XvMCCreateSubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSubp,
                                     int *num_priv, long **priv )
{
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = pI830->xvmc;
    I915XvMCSurfacePriv *sfpriv = NULL;
    XvMCContextPtr ctx = NULL;
    unsigned srfno;
    unsigned bufsize;

    if (I915_XVMC_MAX_SURFACES == pXvMC->nsurfaces) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSubpicture: Too many surfaces !\n");
        return BadAlloc;
    }

    sfpriv = (I915XvMCSurfacePriv *)xcalloc(1, sizeof(I915XvMCSurfacePriv));

    if (!sfpriv) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSubpicture: Unable to allocate memory!\n");
        *num_priv = 0;
        return BadAlloc;
    }

    *priv = (INT32 *)xcalloc(3, sizeof(INT32));

    if (!*priv) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSubpicture: Unable to allocate memory!\n");
        *num_priv = 0;
        xfree(sfpriv);
        return BadAlloc;
    }

    *num_priv = 2;
    ctx = pSubp->context;
    bufsize = size_xx44(ctx->width, ctx->height);

    if (!i830_allocate_xvmc_surface(pScrn, &(sfpriv->surface), bufsize)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "[XvMC] I915XvMCCreateSurface: Failed to allocate XvMC surface space!\n");
        xfree(sfpriv);
        sfpriv = NULL;
        xfree(*priv);
        *priv = NULL;
        *num_priv = 0;
        return BadAlloc;
    }

    for (srfno = 0; srfno < I915_XVMC_MAX_SURFACES; ++srfno) {
        if (0 == pXvMC->sfprivs[srfno])
            break;
    }

    (*priv)[0] = srfno;
    (*priv)[1] = sfpriv->offsets[0] = sfpriv->surface->offset;
    pXvMC->sfprivs[srfno] = sfpriv;
    pXvMC->surfaces[srfno] = pSubp->subpicture_id;
    pXvMC->nsurfaces++;

    return Success;
}

static void I915XvMCDestroyContext (ScrnInfoPtr pScrn, XvMCContextPtr pContext)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = pI830->xvmc;
    int i;

    for (i = 0; i < I915_XVMC_MAX_CONTEXTS; i++) {
        if (pXvMC->contexts[i] == pContext->context_id) {
            i915_unmap_xvmc_buffers(pScrn, pXvMC->ctxprivs[i]);
            i915_free_xvmc_buffers(pScrn, pXvMC->ctxprivs[i]);
            xfree(pXvMC->ctxprivs[i]);
            pXvMC->ctxprivs[i] = 0;
            pXvMC->ncontexts--;
            pXvMC->contexts[i] = 0;
            return;
        }
    }

    return;
}

static void I915XvMCDestroySurface (ScrnInfoPtr pScrn, XvMCSurfacePtr pSurf)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = pI830->xvmc;
    int i;

    for (i = 0; i < I915_XVMC_MAX_SURFACES; i++) {
        if (pXvMC->surfaces[i] == pSurf->surface_id) {
            i830_free_memory(pScrn, pXvMC->sfprivs[i]->surface);            
            xfree(pXvMC->sfprivs[i]);
            pXvMC->nsurfaces--;
            pXvMC->sfprivs[i] = 0;
            pXvMC->surfaces[i] = 0;
            return;
        }
    }

    return;
}

static void I915XvMCDestroySubpicture (ScrnInfoPtr pScrn, XvMCSubpicturePtr pSubp)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = pI830->xvmc;
    int i;

    for (i = 0; i < I915_XVMC_MAX_SURFACES; i++) {
        if (pXvMC->surfaces[i] == pSubp->subpicture_id) {
            i830_free_memory(pScrn, pXvMC->sfprivs[i]->surface);            
            xfree(pXvMC->sfprivs[i]);
            pXvMC->nsurfaces--;
            pXvMC->sfprivs[i] = 0;
            pXvMC->surfaces[i] = 0;
            return;
        }
    }

    return;
}

/*
 *
 */
static int I915XvMCInterceptXvGetAttribute(ScrnInfoPtr pScrn, Atom attribute,
                                           INT32 * value, pointer data)
{
    unsigned i;
    I830PortPrivPtr pPriv = (I830PortPrivPtr)data;
    I915XvMCXVPriv *vx = (I915XvMCXVPriv *)pPriv->xvmc_priv;

    if (I830PTR(pScrn)->XvMCEnabled) {
        for (i = 0; i < vx->xvAttr.numAttr; ++i) {
            if (vx->xvAttr.attributes[i].attribute == attribute) {
                *value = vx->xvAttr.attributes[i].value;
                return Success;
            }
        }
    }

    return vx->GetPortAttribute(pScrn, attribute, value, data);
}

static int I915XvMCInterceptXvAttribute(ScrnInfoPtr pScrn, Atom attribute,
                                        INT32 value, pointer data)
{
    unsigned i;
    I830PortPrivPtr pPriv = (I830PortPrivPtr)data;
    I915XvMCXVPriv *vx = (I915XvMCXVPriv *)pPriv->xvmc_priv;

    if (I830PTR(pScrn)->XvMCEnabled) {
        for (i = 0; i < vx->xvAttr.numAttr; ++i) {
            if (vx->xvAttr.attributes[i].attribute == attribute) {
                vx->xvAttr.attributes[i].value = value;
                return Success;
            }
        }
    }

    return vx->SetPortAttribute(pScrn, attribute, value, data);
}

static int I915XvMCDisplayAttributes(ScrnInfoPtr pScrn,
                                     const I915XvMCAttrHolder * ah, I830PortPrivPtr pPriv)
{
    I915XvMCXVPriv *vx = (I915XvMCXVPriv *) pPriv->xvmc_priv;
    unsigned i;
    int ret;

    for (i = 0; i < ah->numAttr; ++i) {
        ret = vx->SetPortAttribute(pScrn, ah->attributes[i].attribute,
                                   ah->attributes[i].value, pPriv);
        if (ret)
            return ret;
    }

    return Success;
}

static int I915XvMCInterceptPutImage(ScrnInfoPtr pScrn, short src_x, short src_y,
                                     short drw_x, short drw_y, short src_w,
                                     short src_h, short drw_w, short drw_h,
                                     int id, unsigned char *buf, short width,
                                     short height, Bool sync, RegionPtr clipBoxes, pointer data,
                                     DrawablePtr pDraw)
{
    I830PortPrivPtr pPriv = (I830PortPrivPtr)data;
    I915XvMCXVPriv *vx = (I915XvMCXVPriv *)pPriv->xvmc_priv;

    if (I830PTR(pScrn)->XvMCEnabled) {
        if (FOURCC_XVMC == id) {
            I830Ptr pI830 = I830PTR(pScrn);
            I915XvMCPtr pXvMC = pI830->xvmc;
            I915XvMCCommandBuffer *i915XvMCData = (I915XvMCCommandBuffer *)buf;
            int i;

            switch (i915XvMCData->command) {
            case I915_XVMC_COMMAND_ATTRIBUTES:
                if ((i915XvMCData->ctxNo | I915_XVMC_VALID) != vx->ctxDisplaying)
                    return 1;

                I915XvMCDisplayAttributes(pScrn, &i915XvMCData->attrib, pPriv);
                return 0;

            case I915_XVMC_COMMAND_DISPLAY:
                for (i = 0; i < I915_XVMC_MAX_SURFACES; i++) {
                    if (pXvMC->surfaces[i] == i915XvMCData->srfNo) {
                        i830_memory *mem = pXvMC->sfprivs[i]->surface;
                        buf = pI830 + mem->offset;
                        id = i915XvMCData->real_id;
                        break;
                    }
                }

                if (i >= I915_XVMC_MAX_SURFACES)
                    return 1;

                break;

            default:
                return 0;
            }
        }
    }

    return vx->PutImage(pScrn, src_x, src_y, drw_x, drw_y, src_w, src_h,
                        drw_w, drw_h, id, buf, width, height, sync, clipBoxes, data, pDraw);
}

/*********************************** Public Function **************************************/

/**************************************************************************
 *
 *  I915InitMC
 *
 *  Inputs:
 *    Screen pointer
 *
 *  Outputs:
 *    None
 *  
 **************************************************************************/
void I915InitMC(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    I915XvMCPtr pXvMC = NULL; 

    pI830->XvMCEnabled = FALSE;
    return;
    if (!pI830->directRenderingEnabled) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "[XvMC] Cannot use XvMC without DRI!\n");
        return;
    }

    pXvMC = (I915XvMCPtr)calloc(1, sizeof(I915XvMC));
    if (!pXvMC) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "[XvMC] Failure!\n");
        return;
    }

    pI830->xvmc = pXvMC;
    initI915XvMC(pXvMC);
    xf86XvMCScreenInit(pScreen, 1, ppAdapt);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "[XvMC] Initialized XvMC extension.\n");
    pI830->XvMCEnabled = TRUE;
}

int I915XvMCInitXv(ScrnInfoPtr pScrn, XF86VideoAdaptorPtr XvAdapt)
{
    I830PortPrivPtr pPriv;
    I915XvMCXVPriv *vx;
    unsigned i, j;
    SetPortAttributeFuncPtr setportattribute = XvAdapt->SetPortAttribute;
    GetPortAttributeFuncPtr getportattribute = XvAdapt->GetPortAttribute;
    PutImageFuncPtr putimage = XvAdapt->PutImage;

    return Success;
    XvAdapt->GetPortAttribute = I915XvMCInterceptXvGetAttribute;
    XvAdapt->SetPortAttribute = I915XvMCInterceptXvAttribute;
    XvAdapt->PutImage = I915XvMCInterceptPutImage;

    for (j = 0; j < XvAdapt->nPorts; ++j) {
        pPriv = (I830PortPrivPtr) XvAdapt->pPortPrivates[j].ptr;

        if (NULL == (pPriv->xvmc_priv = xcalloc(1, sizeof(I915XvMCXVPriv)))) {
            return BadAlloc;
        }

        for (i = 0; i < I915_NUM_XVMC_ATTRIBUTES; ++i) {
            attrAtoms[i] = MAKE_ATOM(attrXvMC[i]);
        }

        vx = (I915XvMCXVPriv *) pPriv->xvmc_priv;

        vx->ctxDisplaying = 0;
        vx->xvAttr.numAttr = I915_NUM_XVMC_ATTRIBUTES;
        vx->xvmc_port = -1;
        vx->newAttribute = 1;

        /* set up wrappers */
        vx->GetPortAttribute = setportattribute;
        vx->SetPortAttribute = getportattribute;
        vx->PutImage = putimage;

        for (i = 0; i < I915_NUM_XVMC_ATTRIBUTES; ++i) {
            vx->xvAttr.attributes[i].attribute = attrAtoms[i];
            vx->xvAttr.attributes[i].value = 0;
            vx->GetPortAttribute(pScrn, attrAtoms[i],
                                 &(vx->xvAttr.attributes[i].value), pPriv);
        }
    }
    return Success;
}

unsigned long I915XvMCPutImageSize(ScrnInfoPtr pScrn)
{
    if (I830PTR(pScrn)->XvMCEnabled)
        return sizeof(I915XvMCCommandBuffer);

    return 0;
}
