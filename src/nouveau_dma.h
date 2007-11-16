#ifndef __NOUVEAU_DMA_H__
#define __NOUVEAU_DMA_H__

#include <string.h>
#include "nouveau_drmif.h"
#include "nouveau_local.h"

#define RING_SKIPS 8

extern int  nouveau_dma_wait(struct nouveau_channel *chan, int size);
extern void nouveau_dma_subc_bind(struct nouveau_grobj *);
extern void nouveau_dma_channel_init(struct nouveau_channel *);

static inline void
nouveau_dma_out(struct nouveau_channel *userchan, uint32_t data)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);

#ifdef NOUVEAU_DMA_DEBUG
	if (chan->dma.push_free == 0) {
		NOUVEAU_ERR("No space left in packet\n");
		return;
	}
	chan->dma.push_free--;
#endif
#ifdef NOUVEAU_DMA_TRACE
	{
		uint32_t offset = (chan->dma.cur << 2) + chan->dma.base;
		NOUVEAU_MSG("\tOUT_RING %d/0x%08x -> 0x%08x\n",
			    chan->drm.channel, offset, data);
	}
#endif
	chan->pushbuf[chan->dma.cur++] = data;
}

static inline void
nouveau_dma_outp(struct nouveau_channel *userchan, uint32_t *ptr, int size)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	(void)chan;

#ifdef NOUVEAU_DMA_DEBUG
	if (chan->dma.push_free < size) {
		NOUVEAU_ERR("Packet too small.  Free=%d, Need=%d\n",
			    chan->dma.push_free, size);
		return;
	}
#endif
#ifdef NOUVEAU_DMA_TRACE
	while (size--) {
		nouveau_dma_out(userchan, *ptr);
		ptr++;
	}
#else
	memcpy(&chan->pushbuf[chan->dma.cur], ptr, size << 2);
#ifdef NOUVEAU_DMA_DEBUG
	chan->dma.push_free -= size;
#endif
	chan->dma.cur += size;
#endif
}

static inline void
nouveau_dma_begin(struct nouveau_channel *userchan, struct nouveau_grobj *grobj,
		  int method, int size)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	int push_size = size + 1;

#ifdef NOUVEAU_DMA_SUBCHAN_LRU
	if (grobj->bound == NOUVEAU_GROBJ_UNBOUND)
		nouveau_dma_subc_bind(grobj);
	chan->subchannel[grobj->subc].seq = chan->subc_sequence++;
#endif

#ifdef NOUVEAU_DMA_TRACE
	NOUVEAU_MSG("BEGIN_RING %d/%08x/%d/0x%04x/%d\n", chan->drm.channel,
		    grobj->handle, grobj->subc, method, size);
#endif

#ifdef NOUVEAU_DMA_DEUBG
	if (chan->dma.push_free) {
		NOUVEAU_ERR("Previous packet incomplete: %d left\n",
			    chan->dma.push_free);
		return;
	}
#endif
	if (chan->dma.free < push_size) {
#ifdef NOUVEAU_DMA_DEBUG
		if (nouveau_dma_wait(userchan, push_size)) {
			NOUVEAU_ERR("FIFO timeout\n");
			return;
		}
#else
		nouveau_dma_wait(userchan, push_size);
#endif
	}
	chan->dma.free -= push_size;
#ifdef NOUVEAU_DMA_DEBUG
	chan->dma.push_free = push_size;
#endif

	nouveau_dma_out(userchan, (size << 18) | (grobj->subc << 13) | method);
}

static inline uint32_t *
nouveau_dma_beginp(struct nouveau_channel *userchan,
		   struct nouveau_grobj *grobj, int method, int size)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	uint32_t *segment;

	nouveau_dma_begin(userchan, grobj, method, size);

	segment = &chan->pushbuf[chan->dma.cur];
	chan->dma.cur += size;
#ifdef NOUVEAU_DMA_DEBUG
	chan->dma.push_free -= size;
#endif
	return segment;
}

static inline void
nouveau_dma_kickoff(struct nouveau_channel *userchan)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);

#ifdef NOUVEAU_DMA_DEBUG
	if (chan->dma.push_free) {
		NOUVEAU_ERR("Packet incomplete: %d left\n", chan->dma.push_free);
		return;
	}
#endif
	if (chan->dma.cur != chan->dma.put) {
		uint32_t put_offset = (chan->dma.cur << 2) + chan->dma.base;
#ifdef NOUVEAU_DMA_TRACE
		NOUVEAU_MSG("FIRE_RING %d/0x%08x\n", chan->drm.channel,
						     put_offset);
#endif
		chan->dma.put  = chan->dma.cur;
		NOUVEAU_DMA_BARRIER;
		*chan->put     = put_offset;
		NOUVEAU_DMA_BARRIER;
	}
}

static inline void
nouveau_dma_bind(struct nouveau_channel *userchan, struct nouveau_grobj *grobj,
		 int subc)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);

	if (chan->subchannel[subc].grobj == grobj)
		return;

	if (chan->subchannel[subc].grobj)
		chan->subchannel[subc].grobj->bound = NOUVEAU_GROBJ_UNBOUND;
	chan->subchannel[subc].grobj = grobj;
	grobj->subc  = subc;
	grobj->bound = NOUVEAU_GROBJ_EXPLICIT_BIND;

	nouveau_dma_begin(userchan, grobj, 0x0000, 1);
	nouveau_dma_out  (userchan, grobj->handle);
}

#define BIND_RING_CH(ch,gr,sc)       nouveau_dma_bind((ch), (gr), (sc))
#define BEGIN_RING_CH(ch,gr,m,sz)    nouveau_dma_begin((ch), (gr), (m), (sz))
#define OUT_RING_CH(ch, data)        nouveau_dma_out((ch), (data))
#define OUT_RINGp_CH(ch,ptr,dwords)  nouveau_dma_outp((ch), (void*)(ptr),      \
						      (dwords))
#define FIRE_RING_CH(ch)             nouveau_dma_kickoff((ch))
#define WAIT_RING_CH(ch,sz)          nouveau_dma_wait((ch), (sz))
		
#endif
