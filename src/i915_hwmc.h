#ifndef _I915_HWMC_H
#define _I915_HWMC_H

#define STRIDE(w)               (((w) + 0x3ff) & ~0x3ff)
#define SIZE_Y420(w, h)         (h * STRIDE(w))
#define SIZE_UV420(w, h)        ((h >> 1) * STRIDE(w >> 1))
#define SIZE_YUV420(w, h)       (h * (STRIDE(w) + STRIDE(w >> 1)))
#define SIZE_XX44(w, h)         (h * STRIDE(w))

#define FOURCC_XVMC     (('C' << 24) + ('M' << 16) + ('V' << 8) + 'X')

#define I915_NUM_XVMC_ATTRIBUTES       0x02
#define I915_XVMC_VALID 0x80000000

/*
 * Commands that client submits through XvPutImage:
 */

#define I915_XVMC_COMMAND_DISPLAY      0x00
#define I915_XVMC_COMMAND_UNDISPLAY    0x01
#define I915_XVMC_COMMAND_ATTRIBUTES   0x02

typedef struct
{
    INT32 attribute;
    INT32 value;
} I915AttrPair;

typedef struct
{
    unsigned numAttr;
    I915AttrPair attributes[I915_NUM_XVMC_ATTRIBUTES];
} I915XvMCAttrHolder;

typedef struct
{
    unsigned command;
    unsigned ctxNo;
    unsigned srfNo;
    unsigned subPicNo;
    I915XvMCAttrHolder attrib;
    int real_id;
    unsigned pad;
} I915XvMCCommandBuffer;

struct hwmc_buffer
{
    drm_handle_t handle;
    unsigned long offset;
    unsigned long size;
    unsigned long bus_addr;
};

typedef struct 
{
    unsigned ctxno; /* XvMC private context reference number */
    drm_context_t drmcontext;
    struct hwmc_buffer sis;
    struct hwmc_buffer ssb;
    struct hwmc_buffer msb;
    struct hwmc_buffer psp;
    struct hwmc_buffer psc;
    struct hwmc_buffer corrdata;/* Correction Data Buffer */
    struct hwmc_buffer batchbuffer;
    unsigned sarea_size;
    unsigned sarea_priv_offset;
    unsigned screen;
    unsigned depth;
    int deviceID;
    I915XvMCAttrHolder initAttrs;
} I915XvMCCreateContextRec;

typedef struct 
{
    unsigned srfno;
    struct hwmc_buffer srf;
} I915XvMCCreateSurfaceRec;

#endif /* _I915_HWMC_H */
