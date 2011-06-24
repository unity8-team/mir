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
	sna->render.vertex_data[sna->render.vertex_used++] = v;
}
static inline void vertex_emit_2s(struct sna *sna, int16_t x, int16_t y)
{
	int16_t *v = (int16_t *)&sna->render.vertex_data[sna->render.vertex_used++];
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
	return priv && priv->gpu_bo;
}

static inline Bool
is_cpu(DrawablePtr drawable)
{
	struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);
	return !priv || priv->gpu_bo == NULL;
}

static inline Bool
is_dirty_gpu(struct sna *sna, DrawablePtr drawable)
{
	struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);
	return priv && priv->gpu_bo && priv->gpu_damage;
}

static inline Bool
too_small(struct sna *sna, DrawablePtr drawable)
{
	return (drawable->width * drawable->height <= 256) &&
		!is_dirty_gpu(sna, drawable);
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

#endif /* SNA_RENDER_INLINE_H */
