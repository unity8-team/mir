#ifndef GEN4_VERTEX_H
#define GEN4_VERTEX_H

#include "compiler.h"

#include "sna.h"
#include "sna_render.h"

void gen4_vertex_flush(struct sna *sna);
int gen4_vertex_finish(struct sna *sna);
void gen4_vertex_close(struct sna *sna);

inline static uint32_t
gen4_choose_composite_vertex_buffer(const struct sna_composite_op *op)
{
	int id = 2 + !op->src.is_affine;
	if (op->mask.bo)
		id |= (2 + !op->mask.is_affine) << 2;
	DBG(("%s: id=%x (%d, %d)\n", __FUNCTION__, id,
	     2 + !op->src.is_affine,
	     op->mask.bo ?  2 + !op->mask.is_affine : 0));
	assert(id > 0 && id < 16);
	return id;
}

inline inline static uint32_t
gen4_choose_spans_vertex_buffer(const struct sna_composite_op *op)
{
	DBG(("%s: id=%x (%d, 1)\n", __FUNCTION__,
	     1 << 2 | (2+!op->src.is_affine),
	     2 + !op->src.is_affine));
	return 1 << 2 | (2+!op->src.is_affine);
}

void gen4_choose_composite_emitter(struct sna_composite_op *tmp);
void gen4_choose_spans_emitter(struct sna_composite_spans_op *tmp);


#endif /* GEN4_VERTEX_H */
