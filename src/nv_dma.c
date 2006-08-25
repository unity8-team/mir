#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nv_include.h"
#include "nvreg.h"

/* --------------------------------------------------------------------------- */
/* Some documentation of the NVidia DMA command buffers and graphics objects   */
/* --------------------------------------------------------------------------- */

#define HashTableBits 9
#define HashTableSize (1 << HashTableBits)

/* NVidia uses context objects to drive drawing operations.

   Context objects can be selected into 8 subchannels in the FIFO,
   and then used via DMA command buffers.

   A context object is referenced by a user defined handle (CARD32). The HW looks up graphics
   objects in a hash table in the instance RAM.

   An entry in the hash table consists of 2 CARD32. The first CARD32 contains the handle,
   the second one a bitfield, that contains the address of the object in instance RAM.

   The format of the second CARD32 seems to be:

   NV4 to NV30:

   15: 0  instance_addr >> 4
   17:16  engine (here uses 1 = graphics)
   28:24  channel id (here uses 0)
   31	  valid (use 1)

   NV40:

   15: 0  instance_addr >> 4   (maybe 19-0)
   21:20  engine (here uses 1 = graphics)
   I'm unsure about the other bits, but using 0 seems to work.

   The key into the hash table depends on the object handle and channel id and is given as:
*/
static CARD32 hash(CARD32 handle, int chid)
{
	CARD32 result = 0;
	int i;

	for (i = 32; i > 0; i -= HashTableBits) {
		result ^= (handle & ((1 << HashTableBits) - 1));
		handle >>= HashTableBits;
	}
	result ^= chid << (HashTableBits - 4);
	return result << 1;
}


/* Where is the hash table located:

   Base address and size can be calculated from this register:

   ht_base = 0x1000 *  GetBitField (pNv->PFIFO[0x0210/4],8:4);
   ht_size = 0x1000 << GetBitField (pNv->PFIFO[0x0210/4],17:16);

   and the hash table will be located between address PRAMIN + ht_base and
   PRAMIN + ht_base + ht_size.	Each hash table entry has two longwords.

   Please note that PRAMIN starts at 0x700000, whereas the drivers
   PRAMIN pointer starts at 0x710000. Thus we have to subtrace 0x10000
   from these numbers to get the correct offset relative to the PRAMIN
   pointer.
*/
static void initHashTable(NVPtr pNv)
{
    int i;
    const CARD32 offset = 0x10000;
    /* 4k hash table size at 0x10000, search 128 */
    pNv->RAMHT = pNv->PRAMIN;
    pNv->PFIFO[NV_PFIFO_RAMHT/4] = (0x03 << 24) | ((HashTableBits - 9) << 16) | ((offset >> 16) << 4); 
    for (i = 0; i < HashTableSize; ++i)
        pNv->RAMHT[i] = 0;
    /* our first object in instance RAM can be at 0x718000 */
    pNv->pramin_free = 0x1200; 
}


static CARD32 getObject(NVPtr pNv)
{
    CARD32 object = pNv->pramin_free;
    pNv->pramin_free += pNv->Architecture >= NV_ARCH_40 ? 2 : 1;
    return object;
}

/*
   DMA objects are used to reference a piece of memory in the
   framebuffer, PCI or AGP address space. Each object is 16 bytes big
   and looks as follows:
   
   entry[0]
   11:0  class (seems like I can always use 0 here)
   12    page table present?
   13    page entry linear?
   15:14 access: 0 rw, 1 ro, 2 wo
   17:16 target: 0 NV memory, 1 NV memory tiled, 2 PCI, 3 AGP
   31:20 dma adjust (bits 0-11 of the address)
   entry[1]
   dma limit
   entry[2]
   1     0 readonly, 1 readwrite
   31:12 dma frame address (bits 12-31 of the address)

   Non linear page tables seem to need a list of frame addresses afterwards,
   the rivatv project has some info on this.   

   The method below creates a DMA object in instance RAM and returns a handle to it
   that can be used to set up context objects.
*/
CARD32 NVDmaCreateDMAObject(NVPtr pNv, int target, CARD32 base_address, CARD32 size, int access)
{
    /* adjust: adjusts byte offset in a page */
    CARD32 frame_address, adjust, object, pramin_offset;
    if (target == NV_DMA_TARGET_AGP)
        base_address += pNv->agpPhysical;

    frame_address = base_address & ~0xfff;
    adjust = base_address & 0xfff;
    NVDEBUG("NVDmaCreateDMAObject: target = %d, base=%x fram=%x adjust=%x\n", target, base_address, frame_address, adjust);

    /* we take the next empty spot in instance RAM and write our DMA object to it */
    object = getObject(pNv);

    pramin_offset = (object - 0x1000) << 2;

    pNv->PRAMIN[pramin_offset] = (1<<12)|(1<<13)|(adjust<<20)|(access<<14)|(target<<16);
    pNv->PRAMIN[pramin_offset+1] = size - 1;
    pNv->PRAMIN[pramin_offset+2] = frame_address | ((access != NV_DMA_ACCES_RO) ? (1<<1) : 0);
    pNv->PRAMIN[pramin_offset+3] = 0xffffffff;
/*
                                                pNv->Architecture >= NV_ARCH_40
                                   ? 0 : ((access != NV_DMA_ACCES_RO) ? (1<<1) : 0);
*/

    return object;
}

/*
   A DMA notifier is a DMA object that references a small (32 byte it
   seems, we use 256 for saftey) memory area that will be used by the HW to give feedback
   about a DMA operation.
*/
CARD32 NVDmaCreateNotifier(NVPtr pNv, int target, CARD32 base_address)
{
    return NVDmaCreateDMAObject(pNv, target, base_address, 0x100, NV_DMA_ACCES_RW);
}

/*
  How do we wait for DMA completion (by notifiers) ?

   Either repeatedly read the notifier address and wait until it changes,
   or enable a 'wakeup' interrupt by writing NOTIFY_WRITE_LE_AWAKEN into
   the 'notify' field of the object in the channel.  My guess is that 
   this causes an interrupt in PGRAPH/NOTIFY as soon as the transfer is
   completed.  Clients probably can use poll on the nv* devices to get this 
   event.  All this is a guess.	 I don't know any details, and I have not
   tested is.  Also, I have no idea how the 'nvdriver' reacts if it gets 
   notify events that are not registered.

   Writing NV_NOTIFY_WRITE_LE_AWAKEN into the 'Notify' field of an object
   in a channel really causes an interrupt in the PGRAPH engine.  Thus
   we can determine whether a DMA transfer has finished in the interrupt
   handler.

   We can't use interrupts in user land, so we do the simple polling approach.
   The method returns FALSE in case of an error.
*/
Bool NVDmaWaitForNotifier(NVPtr pNv, int target, CARD32 base_address)
{
    volatile U032 *n;
    unsigned char *notifier = (target == NV_DMA_TARGET_AGP)
                              ? pNv->agpMemory
                              : pNv->FbBase;
    notifier += base_address;
    n = (volatile U032 *)notifier;
    NVDEBUG("NVDmaWaitForNotifier @%p", n);
    while (1) {
        U032 a = n[0];
        U032 b = n[1];
        U032 c = n[2];
        U032 status = n[3];
        NVDEBUG("status: n[0]=%x, n[1]=%x, n[2]=%x, n[3]=%x\n", a, b, c, status);
        if (status == 0xffffffff)
            continue;
        if (!status)
            break;
        if (status & 0xffff)
            return FALSE;
    }
    return TRUE;
}

/* Context objects in the instance RAM have the following structure. On NV40 they are 32 byte long,
   on NV30 and smaller 16 bytes.

   NV4 - NV30:

   entry[0]
   11:0 class
   12   chroma key enable
   13   user clip enable
   14   swizzle enable
   17:15 patch config: scrcopy_and, rop_and, blend_and, scrcopy, srccopy_pre, blend_pre
   18   synchronize enable
   19   endian: 1 big, 0 little
   21:20 dither mode
   23    single step enable
   24    patch status: 0 invalid, 1 valid
   25    context_surface 0: 1 valid
   26    context surface 1: 1 valid
   27    context pattern: 1 valid
   28    context rop: 1 valid
   29,30 context beta, beta4
   entry[1]
   7:0   mono format
   15:8  color format
   31:16 notify instance address
   entry[2]
   15:0  dma 0 instance address
   31:16 dma 1 instance address
   entry[3]
   dma method traps

   NV40:
   No idea what the exact format is. Here's what can be deducted:

   entry[0]:
   11:0  class  (maybe uses more bits here?)
   17    user clip enable
   21:19 patch config 
   25    patch status valid ?
   entry[1]:
   15:0  DMA notifier  (maybe 20:0)
   entry[2]:
   15:0  DMA 0 instance (maybe 20:0)
   24    big endian
   entry[3]:
   15:0  DMA 1 instance (maybe 20:0)
   entry[4]:
   entry[5]:
   set to 0?
*/
void NVDmaCreateContextObject(NVPtr pNv, int handle, int class, CARD32 flags,
                              CARD32 dma_in, CARD32 dma_out, CARD32 dma_notifier)
{
    CARD32 pramin_offset;
    CARD32 object = getObject(pNv);
    pramin_offset = (object - 0x1000) << 2;
    NVDEBUG("NVDmaCreateContextObject: storing object at %x\n", pramin_offset);
    
    if (pNv->Architecture >= NV_ARCH_40) {
        CARD32 nv_flags0 = 0;
        CARD32 nv_flags1 = 0;
        CARD32 nv_flags2 = 0;
        if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND)
            nv_flags0 |= 0x02080000;
        else if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY)
            nv_flags0 |= 0x02080000;
        if (flags & NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE)
            nv_flags0 |= 0x00020000;
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x01000000;
        nv_flags2 |= 0x01000000;
#else
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x02000000;
#endif
        pNv->PRAMIN[pramin_offset] = class | nv_flags0;
        pNv->PRAMIN[pramin_offset+1] = dma_notifier | nv_flags1;
        pNv->PRAMIN[pramin_offset+2] = dma_in | nv_flags2;
        pNv->PRAMIN[pramin_offset+3] = dma_out;
        pNv->PRAMIN[pramin_offset+4] = 0;
        pNv->PRAMIN[pramin_offset+5] = 0;
        pNv->PRAMIN[pramin_offset+6] = 0;
        pNv->PRAMIN[pramin_offset+7] = 0;
        
    } else {
        CARD32 nv_flags0 = 0;
        CARD32 nv_flags1 = 0;
        if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND)
            nv_flags0 |= 0x01008000;
        else if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY)
            nv_flags0 |= 0x01018000;
        if (flags & NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE)
            nv_flags0 |= 0x00002000;
#if X_BYTE_ORDER == X_BIG_ENDIAN
        nv_flags0 |= 0x00080000;
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x00000001;
#else
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x00000002;
#endif
        pNv->PRAMIN[pramin_offset] = class | nv_flags0;
        pNv->PRAMIN[pramin_offset+1] = (dma_notifier << 16) | nv_flags1;
        pNv->PRAMIN[pramin_offset+2] = dma_in | (dma_out << 16);
        pNv->PRAMIN[pramin_offset+3] = 0;

    }

    /* insert the created object into the hash table */
    CARD32 h = hash(handle, 0);
    NVDEBUG("storing object %x at hash table offset %d\n", handle, h);
    while (pNv->RAMHT[h]) {
        h += 2;
        if (h == HashTableSize)
            h = 0;
    }
    pNv->RAMHT[h] = handle;
    if (pNv->Architecture >= NV_ARCH_40) {
        pNv->RAMHT[h+1] = (1<<20) | object;
    } else {
        pNv->RAMHT[h+1] = (1<<31) | (1<<16) | object;
    }
}
                           

#if 0
/* Below is the basic structure of DMA command buffers */
#define NV_FIFO_DMA_OPCODE                    ( 0*32+31):( 0*32+29) /* ...VF */
#define NV_FIFO_DMA_OPCODE_METHOD                        0x00000000 /* ...-V */
#define NV_FIFO_DMA_OPCODE_JUMP                          0x00000001 /* ...-V */
#define NV_FIFO_DMA_OPCODE_NONINC_METHOD                 0x00000002 /* ...-V */
#define NV_FIFO_DMA_OPCODE_CALL                          0x00000003 /* ...-V */
#define NV_FIFO_DMA_METHOD_COUNT              ( 0*32+28):( 0*32+18) /* ...VF */
#define NV_FIFO_DMA_METHOD_SUBCHANNEL         ( 0*32+15):( 0*32+13) /* ...VF */
#define NV_FIFO_DMA_METHOD_ADDRESS            ( 0*32+12):( 0*32+ 2) /* ...VF */
#define NV_FIFO_DMA_DATA                      ( 1*32+31):( 1*32+ 0) /* ...VF */
#define NV_FIFO_DMA_NOP                                  0x00000000 /* ...-V */
#define NV_FIFO_DMA_OPCODE                    ( 0*32+31):( 0*32+29) /* ...VF */
#define NV_FIFO_DMA_OPCODE_JUMP                          0x00000001 /* ...-V */
#define NV_FIFO_DMA_JUMP_OFFSET                                28:2 /* ...VF */
#define NV_FIFO_DMA_OPCODE                    ( 0*32+31):( 0*32+29) /* ...VF */
#define NV_FIFO_DMA_OPCODE_CALL                          0x00000003 /* ...-V */
#define NV_FIFO_DMA_CALL_OFFSET                                28:2 /* ...VF */
#define NV_FIFO_DMA_RETURN                               0x00020000 /* ...-V */
#endif


void NVInitDma(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 dma_fb;
#ifdef XF86DRI
    CARD32 dma_agp;
    CARD32 dma_notifier;
#endif
    int i;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,"In NVInitDma\n");
    NVDEBUG("\nNVInitDma!\n");
    initHashTable(pNv); 
    for (i = ((pNv->pramin_free - 0x1000) << 2); i < 0x1000; ++i)
        pNv->PRAMIN[i] = 0;

    dma_fb = NVDmaCreateDMAObject(pNv, NV_DMA_TARGET_VIDMEM, 0, pNv->FbMapSize, NV_DMA_ACCES_RW);
    
    NVDmaCreateContextObject (pNv, NvContextSurfaces,
                              (pNv->Architecture >= NV_ARCH_10) ? NV10_CONTEXT_SURFACES_2D : NV4_SURFACE,
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              dma_fb, dma_fb, 0);
    NVDmaCreateContextObject (pNv, NvRop,
                              NV_ROP5_SOLID, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvImagePattern,
                              NV4_IMAGE_PATTERN, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_MONO,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvClipRectangle,
                              NV_IMAGE_BLACK_RECTANGLE, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvSolidLine,
                              NV4_RENDER_SOLID_LIN, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvImageBlit,
                              pNv->WaitVSyncPossible ? NV12_IMAGE_BLIT : NV_IMAGE_BLIT,
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND, 
                              dma_fb, dma_fb, 0);
    NVDmaCreateContextObject (pNv, NvRectangle,
                              NV4_GDI_RECTANGLE_TEXT, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_MONO, 
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvScaledImage,
                              NV_SCALED_IMAGE_FROM_MEMORY, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY, 
                              dma_fb, dma_fb, 0);

#ifdef XF86DRI
    if (NVDRIScreenInit(pScrn) && NVInitAGP(pScrn) && pNv->agpMemory) {
        dma_agp = NVDmaCreateDMAObject(pNv, NV_DMA_TARGET_AGP, 0x10000, pNv->agpSize - 0x10000,
                                       NV_DMA_ACCES_RW);
        dma_notifier = NVDmaCreateNotifier(pNv, NV_DMA_TARGET_AGP, 0);
        
        NVDmaCreateContextObject (pNv, NvGraphicsToAGP,
                                  NV_MEMORY_TO_MEMORY_FORMAT,
                                  0,
                                  dma_fb, dma_agp, dma_notifier);
        
        NVDmaCreateContextObject (pNv, NvAGPToGraphics,
                                  NV_MEMORY_TO_MEMORY_FORMAT,
                                  0,
                                  dma_agp, dma_fb, dma_notifier);
    }
#endif
    
#if 0
    {
        int i;
        ErrorF("Hash table:\n");
        for (i = 0; i < HashTableSize; i += 2) 
            ErrorF("    %x %x\n", pNv->RAMHT[i], pNv->RAMHT[i+1]);
        ErrorF("Context/DMA objects:\n");
        for (i = 0x800; i < 0x900; i += 8) {
            ErrorF("%x:    %x %x %x %x\n", i,  /*(i*4 + 0x10000)/16, */
                    pNv->RAMHT[i], pNv->RAMHT[i+1], pNv->RAMHT[i+2], pNv->RAMHT[i+3]);
            ErrorF("       %x %x %x %x\n",
                   pNv->RAMHT[i+4], pNv->RAMHT[i+5], pNv->RAMHT[i+6], pNv->RAMHT[i+7]);
        }

    }
#endif
}
