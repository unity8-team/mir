#ifndef __NOUVEAU_LOCAL_H__
#define __NOUVEAU_LOCAL_H__

#include "compiler.h"
#include "xf86_OSproc.h"

/* Debug output */
#define NOUVEAU_MSG(fmt,args...) ErrorF(fmt, ##args)
#define NOUVEAU_ERR(fmt,args...) \
	ErrorF("%s:%d - "fmt, __func__, __LINE__, ##args)

#define NOUVEAU_TIME_MSEC() GetTimeInMillis()

/* User FIFO control */
//#define NOUVEAU_DMA_TRACE
//#define NOUVEAU_DMA_DEBUG
#define NOUVEAU_DMA_SUBCHAN_LRU
#define NOUVEAU_DMA_BARRIER mem_barrier();
#define NOUVEAU_DMA_TIMEOUT 2000

/* Push buffer access macros */
#define BEGIN_RING(obj,mthd,size) do {                                         \
	BEGIN_RING_CH(pNv->chan, pNv->obj, (mthd), (size));                    \
} while(0)

#define OUT_RING(data) do {                                                    \
	OUT_RING_CH(pNv->chan, (data));                                        \
} while(0)

#define OUT_RINGp(src,size) do {                                               \
	OUT_RINGp_CH(pNv->chan, (src), (size));                                \
} while(0)

#define OUT_RINGf(data) do {                                                   \
	union { float v; uint32_t u; } c;                                      \
	c.v = (data);                                                          \
	OUT_RING(c.u);                                                         \
} while(0)

#define WAIT_RING(size) do {                                                   \
	WAIT_RING_CH(pNv->chan, (size));                                       \
} while(0)

#define FIRE_RING() do {                                                       \
	FIRE_RING_CH(pNv->chan);                                               \
} while(0)

#endif
