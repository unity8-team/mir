/*
 * Copyright 2007 Nouveau Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "nouveau_drmif.h"
#include "nouveau_dma.h"
#include "nouveau_local.h"

#define READ_GET(ch) ((*(ch)->get - (ch)->dma.base) >> 2)
#define WRITE_PUT(ch, val) do {                       \
	volatile int dum;                             \
	NOUVEAU_DMA_BARRIER;	                      \
	dum=READ_GET(ch);                             \
	*(ch)->put = (((val) << 2) + (ch)->dma.base); \
	NOUVEAU_DMA_BARRIER;	                      \
} while(0)

#ifdef NOUVEAU_DMA_DEBUG
char faulty[1024];
#endif

void
nouveau_dma_channel_init(struct nouveau_channel *userchan)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	int i;

	chan->dma.base = chan->drm.put_base;
	chan->dma.cur  = chan->dma.put = RING_SKIPS;
	chan->dma.max  = (chan->drm.cmdbuf_size >> 2) - 2;
	chan->dma.free = chan->dma.max - chan->dma.cur;

	for (i = 0; i < RING_SKIPS; i++)
		chan->pushbuf[i] = 0x00000000;
}
/* Reduce the cost of a very short wait, we're still burning cpu though. 
 * It also makes nouveau_dma_wait very visible during profiling.
 */
#define CHECK_TIMEOUT() do {										\
	if (counter == 0) {												\
		t_start = NOUVEAU_TIME_MSEC();								\
		counter++;												\
	} else if (counter < 0xFFFFFFF) {									\
		counter++;												\
	} else if ((NOUVEAU_TIME_MSEC() - t_start) > NOUVEAU_DMA_TIMEOUT) {	\
		return - EBUSY;											\
	}															\
} while(0)

int
nouveau_dma_wait(struct nouveau_channel *userchan, int size)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	uint32_t get, counter = 0, t_start = 0;

	FIRE_RING_CH(userchan);

	while (chan->dma.free < size) {
		get = READ_GET(chan);

		if (chan->dma.put >= get) {
			chan->dma.free = chan->dma.max - chan->dma.cur;

			if (chan->dma.free < size) {
#ifdef NOUVEAU_DMA_DEBUG
				chan->dma.push_free = 1;
#endif
				OUT_RING_CH(userchan,
					    0x20000000 | chan->dma.base);
				if (get <= RING_SKIPS) {
					/*corner case - will be idle*/
					if (chan->dma.put <= RING_SKIPS)
						WRITE_PUT(chan, RING_SKIPS + 1);

					do {
						CHECK_TIMEOUT();
						get = READ_GET(chan);
					} while (get <= RING_SKIPS);
				}

				WRITE_PUT(chan, RING_SKIPS);
				chan->dma.cur  = chan->dma.put = RING_SKIPS;
				chan->dma.free = get - (RING_SKIPS + 1);
			}
		} else {
			chan->dma.free = get - chan->dma.cur - 1;
		}

		CHECK_TIMEOUT();
	}

	return 0;
}

#ifdef NOUVEAU_DMA_SUBCHAN_LRU
void
nouveau_dma_subc_bind(struct nouveau_grobj *grobj)
{
	struct nouveau_channel_priv *chan = nouveau_channel(grobj->channel);
	int subc = -1, i;
	
	for (i = 0; i < 8; i++) {
		if (chan->subchannel[i].grobj &&
		    chan->subchannel[i].grobj->bound == 
		    NOUVEAU_GROBJ_EXPLICIT_BIND)
			continue;
		/* Reading into the -1 index gives undefined results, so pick straight away. */
		if (subc == -1 || chan->subchannel[i].seq < chan->subchannel[subc].seq)
			subc = i;
	}
	assert(subc >= 0);

	if (chan->subchannel[subc].grobj)
		chan->subchannel[subc].grobj->bound = 0;
	chan->subchannel[subc].grobj = grobj;
	grobj->subc  = subc;
	grobj->bound = NOUVEAU_GROBJ_BOUND;

	BEGIN_RING_CH(grobj->channel, grobj, 0, 1);
	nouveau_dma_out  (grobj->channel, grobj->handle);
}
#endif

void
nouveau_dma_kickoff(struct nouveau_channel *userchan)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	uint32_t put_offset;
	int i;

	if (chan->dma.cur == chan->dma.put)
		return;

	if (chan->num_relocs) {
		nouveau_bo_validate(userchan);
		
		for (i = 0; i < chan->num_relocs; i++) {
			struct nouveau_bo_reloc *r = &chan->relocs[i];
			uint32_t push;

			if (r->flags & NOUVEAU_BO_LOW) {
				push = r->bo->base.offset + r->data;
			} else
			if (r->flags & NOUVEAU_BO_HIGH) {
				push = (r->bo->base.offset + r->data) >> 32;
			} else {
				push = r->data;
			}

			if (r->flags & NOUVEAU_BO_OR) {
				if (r->flags & NOUVEAU_BO_VRAM)
					push |= r->vor;
				else
					push |= r->tor;
			}

			*r->ptr = push;
		}

		chan->num_relocs = 0;
	}

#ifdef NOUVEAU_DMA_DEBUG
	if (chan->dma.push_free) {
		NOUVEAU_ERR("Packet incomplete: %d left\n", chan->dma.push_free);
		return;
	}
#endif

	put_offset = (chan->dma.cur << 2) + chan->dma.base;
#ifdef NOUVEAU_DMA_TRACE
	NOUVEAU_MSG("FIRE_RING %d/0x%08x\n", chan->drm.channel, put_offset);
#endif
	chan->dma.put  = chan->dma.cur;
	NOUVEAU_DMA_BARRIER;
	*chan->put     = put_offset;
	NOUVEAU_DMA_BARRIER;
}
