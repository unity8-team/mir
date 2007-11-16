#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "nouveau_drmif.h"
#include "nouveau_dma.h"
#include "nouveau_local.h"

#define READ_GET(ch) ((*(ch)->get - (ch)->dma.base) >> 2)
#define WRITE_PUT(ch, val) do {                       \
	*(ch)->put = (((val) << 2) + (ch)->dma.base); \
} while(0)

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

#define CHECK_TIMEOUT() do {                                                   \
	if ((NOUVEAU_TIME_MSEC() - t_start) > NOUVEAU_DMA_TIMEOUT)             \
		return - EBUSY;                                                \
} while(0)

int
nouveau_dma_wait(struct nouveau_channel *userchan, int size)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	uint32_t get, t_start;

	FIRE_RING_CH(userchan);

	t_start = NOUVEAU_TIME_MSEC();
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
		if (chan->subchannel[i].seq < chan->subchannel[subc].seq)
			subc = i;
	}
	assert(subc >= 0);

	if (chan->subchannel[subc].grobj)
		chan->subchannel[subc].grobj->bound = 0;
	chan->subchannel[subc].grobj = grobj;
	grobj->subc  = subc;
	grobj->bound = NOUVEAU_GROBJ_BOUND;

	nouveau_dma_begin(grobj->channel, grobj, 0, 1);
	nouveau_dma_out  (grobj->channel, grobj->handle);
}
#endif

