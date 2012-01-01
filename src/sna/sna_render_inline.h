#ifndef SNA_RENDER_INLINE_H
#define SNA_RENDER_INLINE_H

static inline bool need_tiling(struct sna *sna, int16_t width, int16_t height)
{
	/* Is the damage area too large to fit in 3D pipeline,
	 * and so do we need to split the operation up into tiles?
	 */
	return (width > sna->render.max_3d_size ||
		height > sna->render.max_3d_size);
}

static inline bool need_redirect(struct sna *sna, PixmapPtr dst)
{
	/* Is the pixmap too large to render to? */
	return (dst->drawable.width > sna->render.max_3d_size ||
		dst->drawable.height > sna->render.max_3d_size);
}

static inline int vertex_space(struct sna *sna)
{
	return ARRAY_SIZE(sna->render.vertex_data) - sna->render.vertex_used;
}
static inline void vertex_emit(struct sna *sna, float v)
{
	assert(sna->render.vertex_used < ARRAY_SIZE(sna->render.vertex_data));
	sna->render.vertex_data[sna->render.vertex_used++] = v;
}
static inline void vertex_emit_2s(struct sna *sna, int16_t x, int16_t y)
{
	int16_t *v = (int16_t *)&sna->render.vertex_data[sna->render.vertex_used++];
	assert(sna->render.vertex_used <= ARRAY_SIZE(sna->render.vertex_data));
	v[0] = x;
	v[1] = y;
}

static inline float pack_2s(int16_t x, int16_t y)
{
	union {
		struct sna_coordinate p;
		float f;
	} u;
	u.p.x = x;
	u.p.y = y;
	return u.f;
}

static inline int batch_space(struct sna *sna)
{
	return KGEM_BATCH_SIZE(&sna->kgem) - sna->kgem.nbatch;
}

static inline void batch_emit(struct sna *sna, uint32_t dword)
{
	assert(sna->kgem.nbatch + KGEM_BATCH_RESERVED < sna->kgem.surface);
	sna->kgem.batch[sna->kgem.nbatch++] = dword;
}

static inline void batch_emit_float(struct sna *sna, float f)
{
	union {
		uint32_t dw;
		float f;
	} u;
	u.f = f;
	batch_emit(sna, u.dw);
}

static inline Bool
is_gpu(DrawablePtr drawable)
{
	struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

	if (priv == NULL)
		return false;

	if (priv->gpu_damage)
		return true;

	return priv->cpu_bo && kgem_bo_is_busy(priv->cpu_bo);
}

static inline Bool
is_cpu(DrawablePtr drawable)
{
	struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);
	return !priv || priv->cpu_damage != NULL;
}

static inline Bool
too_small(DrawablePtr drawable)
{
	return ((uint32_t)drawable->width * drawable->height * drawable->bitsPerPixel <= 8*4096) && !is_gpu(drawable);
}

static inline Bool
picture_is_gpu(PicturePtr picture)
{
	if (!picture || !picture->pDrawable)
		return FALSE;
	return is_gpu(picture->pDrawable);
}

static inline Bool sna_blt_compare_depth(DrawablePtr src, DrawablePtr dst)
{
	if (src->depth == dst->depth)
		return TRUE;

	/* Also allow for the alpha to be discarded on a copy */
	if (src->bitsPerPixel != dst->bitsPerPixel)
		return FALSE;

	if (dst->depth == 24 && src->depth == 32)
		return TRUE;

	/* Note that a depth-16 pixmap is r5g6b5, not x1r5g5b5. */

	return FALSE;
}

static inline struct kgem_bo *
sna_render_get_alpha_gradient(struct sna *sna)
{
	return kgem_bo_reference(sna->render.alpha_cache.cache_bo);
}

static inline void
sna_render_reduce_damage(struct sna_composite_op *op,
			 int dst_x, int dst_y,
			 int width, int height)
{
	BoxRec r;

	if (op->damage == NULL || *op->damage == NULL)
		return;

	if ((*op->damage)->mode == DAMAGE_ALL) {
		op->damage = NULL;
		return;
	}

	if (width == 0 || height == 0)
		return;

	r.x1 = dst_x + op->dst.x;
	r.x2 = r.x1 + width;

	r.y1 = dst_y + op->dst.y;
	r.y2 = r.y1 + height;

	if (sna_damage_contains_box(*op->damage, &r) == PIXMAN_REGION_IN)
		op->damage = NULL;
}

#endif /* SNA_RENDER_INLINE_H */
