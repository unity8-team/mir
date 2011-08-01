/*
 * Copyright (c) 2007  David Turner
 * Copyright (c) 2008  M Joonas Pihlaja
 * Copyright (c) 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_render.h"
#include "sna_render_inline.h"

#include <mipict.h>
#include <fbpict.h>

#if DEBUG_TRAPEZOIDS
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define NO_ACCEL 0
#define NO_ALIGNED_BOXES 0
#define NO_UNALIGNED_BOXES 0
#define NO_SCAN_CONVERTER 0

#define unlikely(x) x

#define SAMPLES_X 17
#define SAMPLES_Y 15

#define FAST_SAMPLES_X_shift 8
#define FAST_SAMPLES_Y_shift 4

#define FAST_SAMPLES_X (1<<FAST_SAMPLES_X_shift)
#define FAST_SAMPLES_Y (1<<FAST_SAMPLES_Y_shift)

#if DEBUG_TRAPEZOIDS
static void _assert_pixmap_contains_box(PixmapPtr pixmap, BoxPtr box, const char *function)
{
	if (box->x1 < 0 || box->y1 < 0 ||
	    box->x2 > pixmap->drawable.width ||
	    box->y2 > pixmap->drawable.height)
	{
		ErrorF("%s: damage box is beyond the pixmap: box=(%d, %d), (%d, %d), pixmap=(%d, %d)\n",
		       __FUNCTION__,
		       box->x1, box->y1, box->x2, box->y2,
		       pixmap->drawable.width,
		       pixmap->drawable.height);
		assert(0);
	}
}
#define assert_pixmap_contains_box(p, b) _assert_pixmap_contains_box(p, b, __FUNCTION__)
#else
#define assert_pixmap_contains_box(p, b)
#endif

static void apply_damage(struct sna_composite_op *op, RegionPtr region)
{
	DBG(("%s: damage=%p, region=%d\n",
	     __FUNCTION__, op->damage, REGION_NUM_RECTS(region)));

	if (op->damage == NULL)
		return;

	RegionTranslate(region, op->dst.x, op->dst.y);

	assert_pixmap_contains_box(op->dst.pixmap, RegionExtents(region));
	sna_damage_add(op->damage, region);
}

static void apply_damage_box(struct sna_composite_op *op, const BoxRec *box)
{
	BoxRec r;

	if (op->damage == NULL)
		return;

	r.x1 = box->x1 + op->dst.x;
	r.x2 = box->x2 + op->dst.x;
	r.y1 = box->y1 + op->dst.y;
	r.y2 = box->y2 + op->dst.y;

	assert_pixmap_contains_box(op->dst.pixmap, &r);
	sna_damage_add_box(op->damage, &r);
}

typedef int grid_scaled_x_t;
typedef int grid_scaled_y_t;

#define FAST_SAMPLES_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_shift(x, i, f, FAST_SAMPLES_X_shift)

#define _GRID_TO_INT_FRAC_shift(t, i, f, b) do {	\
    (f) = (t) & ((1 << (b)) - 1);			\
    (i) = (t) >> (b);					\
} while (0)

/* A grid area is a real in [0,1] scaled by 2*SAMPLES_X*SAMPLES_Y.  We want
 * to be able to represent exactly areas of subpixel trapezoids whose
 * vertices are given in grid scaled coordinates.  The scale factor
 * comes from needing to accurately represent the area 0.5*dx*dy of a
 * triangle with base dx and height dy in grid scaled numbers. */
typedef int grid_area_t;
#define FAST_SAMPLES_XY (2*FAST_SAMPLES_X*FAST_SAMPLES_Y) /* Unit area on the grid. */

#define AREA_TO_ALPHA(c)  ((c) / (float)FAST_SAMPLES_XY)

struct quorem {
	int32_t quo;
	int32_t rem;
};

struct _pool_chunk {
	size_t size;
	size_t capacity;

	struct _pool_chunk *prev_chunk;
	/* Actual data starts here.	 Well aligned for pointers. */
};

/* A memory pool.  This is supposed to be embedded on the stack or
 * within some other structure.	 It may optionally be followed by an
 * embedded array from which requests are fulfilled until
 * malloc needs to be called to allocate a first real chunk. */
struct pool {
	struct _pool_chunk *current;
	struct _pool_chunk *first_free;

	/* The default capacity of a chunk. */
	size_t default_capacity;

	/* Header for the sentinel chunk.  Directly following the pool
	 * struct should be some space for embedded elements from which
	 * the sentinel chunk allocates from. */
	struct _pool_chunk sentinel[1];
};

/* A polygon edge. */
struct edge {
	/* Next in y-bucket or active list. */
	struct edge *next;

	/* Current x coordinate while the edge is on the active
	 * list. Initialised to the x coordinate of the top of the
	 * edge. The quotient is in grid_scaled_x_t units and the
	 * remainder is mod dy in grid_scaled_y_t units.*/
	struct quorem x;

	/* Advance of the current x when moving down a subsample line. */
	struct quorem dxdy;

	/* Advance of the current x when moving down a full pixel
	 * row. Only initialised when the height of the edge is large
	 * enough that there's a chance the edge could be stepped by a
	 * full row's worth of subsample rows at a time. */
	struct quorem dxdy_full;

	/* The clipped y of the top of the edge. */
	grid_scaled_y_t ytop;

	/* y2-y1 after orienting the edge downwards.  */
	grid_scaled_y_t dy;

	/* Number of subsample rows remaining to scan convert of this
	 * edge. */
	grid_scaled_y_t height_left;

	/* Original sign of the edge: +1 for downwards, -1 for upwards
	 * edges.  */
	int dir;
	int vertical;
};

/* Number of subsample rows per y-bucket. Must be SAMPLES_Y. */
#define EDGE_Y_BUCKET_HEIGHT FAST_SAMPLES_Y
#define EDGE_Y_BUCKET_INDEX(y, ymin) (((y) - (ymin))/EDGE_Y_BUCKET_HEIGHT)

/* A collection of sorted and vertically clipped edges of the polygon.
 * Edges are moved from the polygon to an active list while scan
 * converting. */
struct polygon {
	/* The vertical clip extents. */
	grid_scaled_y_t ymin, ymax;

	/* Array of edges all starting in the same bucket.	An edge is put
	 * into bucket EDGE_BUCKET_INDEX(edge->ytop, polygon->ymin) when
	 * it is added to the polygon. */
	struct edge **y_buckets;
	struct edge *y_buckets_embedded[64];

	struct edge edges_embedded[32];
	struct edge *edges;
	int num_edges;
};

/* A cell records the effect on pixel coverage of polygon edges
 * passing through a pixel.  It contains two accumulators of pixel
 * coverage.
 *
 * Consider the effects of a polygon edge on the coverage of a pixel
 * it intersects and that of the following one.  The coverage of the
 * following pixel is the height of the edge multiplied by the width
 * of the pixel, and the coverage of the pixel itself is the area of
 * the trapezoid formed by the edge and the right side of the pixel.
 *
 * +-----------------------+-----------------------+
 * |                       |                       |
 * |                       |                       |
 * |_______________________|_______________________|
 * |   \...................|.......................|\
 * |    \..................|.......................| |
 * |     \.................|.......................| |
 * |      \....covered.....|.......................| |
 * |       \....area.......|.......................| } covered height
 * |        \..............|.......................| |
 * |uncovered\.............|.......................| |
 * |  area    \............|.......................| |
 * |___________\...........|.......................|/
 * |                       |                       |
 * |                       |                       |
 * |                       |                       |
 * +-----------------------+-----------------------+
 *
 * Since the coverage of the following pixel will always be a multiple
 * of the width of the pixel, we can store the height of the covered
 * area instead.  The coverage of the pixel itself is the total
 * coverage minus the area of the uncovered area to the left of the
 * edge.  As it's faster to compute the uncovered area we only store
 * that and subtract it from the total coverage later when forming
 * spans to blit.
 *
 * The heights and areas are signed, with left edges of the polygon
 * having positive sign and right edges having negative sign.  When
 * two edges intersect they swap their left/rightness so their
 * contribution above and below the intersection point must be
 * computed separately. */
struct cell {
	struct cell *next;
	int x;
	grid_area_t uncovered_area;
	grid_scaled_y_t covered_height;
};

/* A cell list represents the scan line sparsely as cells ordered by
 * ascending x.  It is geared towards scanning the cells in order
 * using an internal cursor. */
struct cell_list {
	/* Points to the left-most cell in the scan line. */
	struct cell *head;
	/* Sentinel node */
	struct cell tail;

	struct cell **cursor;

	/* Cells in the cell list are owned by the cell list and are
	 * allocated from this pool.  */
	struct {
		struct pool base[1];
		struct cell embedded[32];
	} cell_pool;
};

/* The active list contains edges in the current scan line ordered by
 * the x-coordinate of the intercept of the edge and the scan line. */
struct active_list {
	/* Leftmost edge on the current scan line. */
	struct edge *head;

	/* A lower bound on the height of the active edges is used to
	 * estimate how soon some active edge ends.	 We can't advance the
	 * scan conversion by a full pixel row if an edge ends somewhere
	 * within it. */
	grid_scaled_y_t min_height;
};

struct tor {
    struct polygon	polygon[1];
    struct active_list	active[1];
    struct cell_list	coverages[1];

    /* Clip box. */
    grid_scaled_x_t xmin, xmax;
    grid_scaled_y_t ymin, ymax;
};

/* Compute the floored division a/b. Assumes / and % perform symmetric
 * division. */
inline static struct quorem
floored_divrem(int a, int b)
{
	struct quorem qr;
	qr.quo = a/b;
	qr.rem = a%b;
	if ((a^b)<0 && qr.rem) {
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

/* Compute the floored division (x*a)/b. Assumes / and % perform symmetric
 * division. */
static struct quorem
floored_muldivrem(int x, int a, int b)
{
	struct quorem qr;
	long long xa = (long long)x*a;
	qr.quo = xa/b;
	qr.rem = xa%b;
	if ((xa>=0) != (b>=0) && qr.rem) {
		qr.quo -= 1;
		qr.rem += b;
	}
	return qr;
}

static void
_pool_chunk_init(
    struct _pool_chunk *p,
    struct _pool_chunk *prev_chunk,
    size_t capacity)
{
	p->prev_chunk = prev_chunk;
	p->size = 0;
	p->capacity = capacity;
}

static struct _pool_chunk *
_pool_chunk_create(struct _pool_chunk *prev_chunk, size_t size)
{
	struct _pool_chunk *p;
	size_t size_with_head = size + sizeof(struct _pool_chunk);

	if (size_with_head < size)
		return NULL;

	p = malloc(size_with_head);
	if (p)
		_pool_chunk_init(p, prev_chunk, size);

	return p;
}

static void
pool_init(struct pool *pool,
	  size_t default_capacity,
	  size_t embedded_capacity)
{
	pool->current = pool->sentinel;
	pool->first_free = NULL;
	pool->default_capacity = default_capacity;
	_pool_chunk_init(pool->sentinel, NULL, embedded_capacity);
}

static void
pool_fini(struct pool *pool)
{
	struct _pool_chunk *p = pool->current;
	do {
		while (NULL != p) {
			struct _pool_chunk *prev = p->prev_chunk;
			if (p != pool->sentinel)
				free(p);
			p = prev;
		}
		p = pool->first_free;
		pool->first_free = NULL;
	} while (NULL != p);
	pool_init(pool, 0, 0);
}

/* Satisfy an allocation by first allocating a new large enough chunk
 * and adding it to the head of the pool's chunk list. This function
 * is called as a fallback if pool_alloc() couldn't do a quick
 * allocation from the current chunk in the pool. */
static void *
_pool_alloc_from_new_chunk(struct pool *pool, size_t size)
{
	struct _pool_chunk *chunk;
	void *obj;
	size_t capacity;

	/* If the allocation is smaller than the default chunk size then
	 * try getting a chunk off the free list.  Force alloc of a new
	 * chunk for large requests. */
	capacity = size;
	chunk = NULL;
	if (size < pool->default_capacity) {
		capacity = pool->default_capacity;
		chunk = pool->first_free;
		if (chunk) {
			pool->first_free = chunk->prev_chunk;
			_pool_chunk_init(chunk, pool->current, chunk->capacity);
		}
	}

	if (NULL == chunk) {
		chunk = _pool_chunk_create (pool->current, capacity);
		if (unlikely (NULL == chunk))
			return NULL;
	}
	pool->current = chunk;

	obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
	chunk->size += size;
	return obj;
}

inline static void *
pool_alloc(struct pool *pool, size_t size)
{
	struct _pool_chunk *chunk = pool->current;

	if (size <= chunk->capacity - chunk->size) {
		void *obj = ((unsigned char*)chunk + sizeof(*chunk) + chunk->size);
		chunk->size += size;
		return obj;
	} else
		return _pool_alloc_from_new_chunk(pool, size);
}

static void
pool_reset(struct pool *pool)
{
	/* Transfer all used chunks to the chunk free list. */
	struct _pool_chunk *chunk = pool->current;
	if (chunk != pool->sentinel) {
		while (chunk->prev_chunk != pool->sentinel)
			chunk = chunk->prev_chunk;

		chunk->prev_chunk = pool->first_free;
		pool->first_free = pool->current;
	}

	/* Reset the sentinel as the current chunk. */
	pool->current = pool->sentinel;
	pool->sentinel->size = 0;
}

/* Rewinds the cell list's cursor to the beginning.  After rewinding
 * we're good to cell_list_find() the cell any x coordinate. */
inline static void
cell_list_rewind(struct cell_list *cells)
{
	cells->cursor = &cells->head;
}

/* Rewind the cell list if its cursor has been advanced past x. */
inline static void
cell_list_maybe_rewind(struct cell_list *cells, int x)
{
	if ((*cells->cursor)->x > x)
		cell_list_rewind(cells);
}

static void
cell_list_init(struct cell_list *cells)
{
	pool_init(cells->cell_pool.base,
		  256*sizeof(struct cell),
		  sizeof(cells->cell_pool.embedded));
	cells->tail.next = NULL;
	cells->tail.x = INT_MAX;
	cells->head = &cells->tail;
	cell_list_rewind(cells);
}

static void
cell_list_fini(struct cell_list *cells)
{
	pool_fini(cells->cell_pool.base);
}

inline static void
cell_list_reset(struct cell_list *cells)
{
	cell_list_rewind(cells);
	cells->head = &cells->tail;
	pool_reset(cells->cell_pool.base);
}

static struct cell *
cell_list_alloc(struct cell_list *cells,
		struct cell *tail,
		int x)
{
	struct cell *cell;

	cell = pool_alloc(cells->cell_pool.base, sizeof (struct cell));
	if (unlikely(NULL == cell))
		abort();

	*cells->cursor = cell;
	cell->next = tail;
	cell->x = x;
	cell->uncovered_area = 0;
	cell->covered_height = 0;
	return cell;
}

/* Find a cell at the given x-coordinate.  Returns %NULL if a new cell
 * needed to be allocated but couldn't be.  Cells must be found with
 * non-decreasing x-coordinate until the cell list is rewound using
 * cell_list_rewind(). Ownership of the returned cell is retained by
 * the cell list. */
inline static struct cell *
cell_list_find(struct cell_list *cells, int x)
{
	struct cell **cursor = cells->cursor;
	struct cell *cell;

	do {
		cell = *cursor;
		if (cell->x >= x)
			break;

		cursor = &cell->next;
	} while (1);
	cells->cursor = cursor;

	if (cell->x == x)
		return cell;

	return cell_list_alloc(cells, cell, x);
}

/* Add an unbounded subpixel span covering subpixels >= x to the
 * coverage cells. */
static void
cell_list_add_unbounded_subspan(struct cell_list *cells, grid_scaled_x_t x)
{
	struct cell *cell;
	int ix, fx;

	FAST_SAMPLES_X_TO_INT_FRAC(x, ix, fx);

	DBG(("%s: x=%d (%d+%d)\n", __FUNCTION__, x, ix, fx));

	cell = cell_list_find(cells, ix);
	cell->uncovered_area += 2*fx;
	cell->covered_height++;
}

/* Add a subpixel span covering [x1, x2) to the coverage cells. */
inline static void
cell_list_add_subspan(struct cell_list *cells,
		      grid_scaled_x_t x1,
		      grid_scaled_x_t x2)
{
	struct cell *cell;
	int ix1, fx1;
	int ix2, fx2;

	FAST_SAMPLES_X_TO_INT_FRAC(x1, ix1, fx1);
	FAST_SAMPLES_X_TO_INT_FRAC(x2, ix2, fx2);

	DBG(("%s: x1=%d (%d+%d), x2=%d (%d+%d)\n", __FUNCTION__,
	     x1, ix1, fx1, x2, ix2, fx2));

	cell = cell_list_find(cells, ix1);
	if (ix1 != ix2) {
		cell->uncovered_area += 2*fx1;
		++cell->covered_height;

		cell = cell_list_find(cells, ix2);
		cell->uncovered_area -= 2*fx2;
		--cell->covered_height;
	} else
		cell->uncovered_area += 2*(fx1-fx2);
}

/* Adds the analytical coverage of an edge crossing the current pixel
 * row to the coverage cells and advances the edge's x position to the
 * following row.
 *
 * This function is only called when we know that during this pixel row:
 *
 * 1) The relative order of all edges on the active list doesn't
 * change.  In particular, no edges intersect within this row to pixel
 * precision.
 *
 * 2) No new edges start in this row.
 *
 * 3) No existing edges end mid-row.
 *
 * This function depends on being called with all edges from the
 * active list in the order they appear on the list (i.e. with
 * non-decreasing x-coordinate.)  */
static void
cell_list_render_edge(struct cell_list *cells, struct edge *edge, int sign)
{
	grid_scaled_y_t y1, y2, dy;
	grid_scaled_x_t fx1, fx2, dx;
	int ix1, ix2;
	struct quorem x1 = edge->x;
	struct quorem x2 = x1;

	if (!edge->vertical) {
		x2.quo += edge->dxdy_full.quo;
		x2.rem += edge->dxdy_full.rem;
		if (x2.rem >= 0) {
			++x2.quo;
			x2.rem -= edge->dy;
		}

		edge->x = x2;
	}

	FAST_SAMPLES_X_TO_INT_FRAC(x1.quo, ix1, fx1);
	FAST_SAMPLES_X_TO_INT_FRAC(x2.quo, ix2, fx2);

	DBG(("%s: x1=%d (%d+%d), x2=%d (%d+%d)\n", __FUNCTION__,
	     x1.quo, ix1, fx1, x2.quo, ix2, fx2));

	/* Edge is entirely within a column? */
	if (ix1 == ix2) {
		/* We always know that ix1 is >= the cell list cursor in this
		 * case due to the no-intersections precondition.  */
		struct cell *cell = cell_list_find(cells, ix1);
		cell->covered_height += sign*FAST_SAMPLES_Y;
		cell->uncovered_area += sign*(fx1 + fx2)*FAST_SAMPLES_Y;
		return;
	}

	/* Orient the edge left-to-right. */
	dx = x2.quo - x1.quo;
	if (dx >= 0) {
		y1 = 0;
		y2 = FAST_SAMPLES_Y;
	} else {
		int tmp;
		tmp = ix1; ix1 = ix2; ix2 = tmp;
		tmp = fx1; fx1 = fx2; fx2 = tmp;
		dx = -dx;
		sign = -sign;
		y1 = FAST_SAMPLES_Y;
		y2 = 0;
	}
	dy = y2 - y1;

	/* Add coverage for all pixels [ix1,ix2] on this row crossed
	 * by the edge. */
	{
		struct quorem y = floored_divrem((FAST_SAMPLES_X - fx1)*dy, dx);
		struct cell *cell;

		/* When rendering a previous edge on the active list we may
		 * advance the cell list cursor past the leftmost pixel of the
		 * current edge even though the two edges don't intersect.
		 * e.g. consider two edges going down and rightwards:
		 *
		 *  --\_+---\_+-----+-----+----
		 *      \_    \_    |     |
		 *      | \_  | \_  |     |
		 *      |   \_|   \_|     |
		 *      |     \_    \_    |
		 *  ----+-----+-\---+-\---+----
		 *
		 * The left edge touches cells past the starting cell of the
		 * right edge.  Fortunately such cases are rare.
		 *
		 * The rewinding is never necessary if the current edge stays
		 * within a single column because we've checked before calling
		 * this function that the active list order won't change. */
		cell_list_maybe_rewind(cells, ix1);

		cell = cell_list_find(cells, ix1);
		cell->uncovered_area += sign*y.quo*(FAST_SAMPLES_X + fx1);
		cell->covered_height += sign*y.quo;
		y.quo += y1;

		cell = cell_list_find(cells, ++ix1);
		if (ix1 < ix2) {
			struct quorem dydx_full = floored_divrem(FAST_SAMPLES_X*dy, dx);
			do {
				grid_scaled_y_t y_skip = dydx_full.quo;
				y.rem += dydx_full.rem;
				if (y.rem >= dx) {
					++y_skip;
					y.rem -= dx;
				}

				y.quo += y_skip;

				y_skip *= sign;
				cell->uncovered_area += y_skip*FAST_SAMPLES_X;
				cell->covered_height += y_skip;

				cell = cell_list_find(cells, ++ix1);
			} while (ix1 != ix2);
		}
		cell->uncovered_area += sign*(y2 - y.quo)*fx2;
		cell->covered_height += sign*(y2 - y.quo);
	}
}


static void
polygon_fini(struct polygon *polygon)
{
	if (polygon->y_buckets != polygon->y_buckets_embedded)
		free(polygon->y_buckets);

	if (polygon->edges != polygon->edges_embedded)
		free(polygon->edges);
}

static int
polygon_init(struct polygon *polygon,
	     int num_edges,
	     grid_scaled_y_t ymin,
	     grid_scaled_y_t ymax)
{
	unsigned h = ymax - ymin;
	unsigned num_buckets = EDGE_Y_BUCKET_INDEX(ymax+EDGE_Y_BUCKET_HEIGHT-1,
						   ymin);

	if (unlikely(h > 0x7FFFFFFFU - EDGE_Y_BUCKET_HEIGHT))
		goto bail_no_mem; /* even if you could, you wouldn't want to. */

	polygon->edges = polygon->edges_embedded;
	polygon->y_buckets = polygon->y_buckets_embedded;

	polygon->num_edges = 0;
	if (num_edges > ARRAY_SIZE(polygon->edges_embedded)) {
		polygon->edges = malloc(sizeof(struct edge)*num_edges);
		if (unlikely(NULL == polygon->edges))
			goto bail_no_mem;
	}

	if (num_buckets > ARRAY_SIZE(polygon->y_buckets_embedded)) {
		polygon->y_buckets = malloc(num_buckets*sizeof(struct edge *));
		if (unlikely(NULL == polygon->y_buckets))
			goto bail_no_mem;
	}
	memset(polygon->y_buckets, 0, num_buckets * sizeof(struct edge *));

	polygon->ymin = ymin;
	polygon->ymax = ymax;
	return 0;

bail_no_mem:
	polygon_fini(polygon);
	return -1;
}

static void
_polygon_insert_edge_into_its_y_bucket(struct polygon *polygon, struct edge *e)
{
	unsigned ix = EDGE_Y_BUCKET_INDEX(e->ytop, polygon->ymin);
	struct edge **ptail = &polygon->y_buckets[ix];
	e->next = *ptail;
	*ptail = e;
}

inline static void
polygon_add_edge(struct polygon *polygon,
		 grid_scaled_x_t x1,
		 grid_scaled_x_t x2,
		 grid_scaled_y_t y1,
		 grid_scaled_y_t y2,
		 grid_scaled_y_t top,
		 grid_scaled_y_t bottom,
		 int dir)
{
	struct edge *e = &polygon->edges[polygon->num_edges++];
	grid_scaled_x_t dx = x2 - x1;
	grid_scaled_y_t dy = y2 - y1;
	grid_scaled_y_t ytop, ybot;
	grid_scaled_y_t ymin = polygon->ymin;
	grid_scaled_y_t ymax = polygon->ymax;

	e->dy = dy;
	e->dir = dir;

	ytop = top >= ymin ? top : ymin;
	ybot = bottom <= ymax ? bottom : ymax;
	e->ytop = ytop;
	e->height_left = ybot - ytop;

	if (dx == 0) {
		e->vertical = true;
		e->x.quo = x1;
		e->x.rem = 0;
		e->dxdy.quo = 0;
		e->dxdy.rem = 0;
		e->dxdy_full.quo = 0;
		e->dxdy_full.rem = 0;
	} else {
		e->vertical = false;
		e->dxdy = floored_divrem(dx, dy);
		if (ytop == y1) {
			e->x.quo = x1;
			e->x.rem = 0;
		} else {
			e->x = floored_muldivrem(ytop - y1, dx, dy);
			e->x.quo += x1;
		}

		if (e->height_left >= FAST_SAMPLES_Y) {
			e->dxdy_full = floored_muldivrem(FAST_SAMPLES_Y, dx, dy);
		} else {
			e->dxdy_full.quo = 0;
			e->dxdy_full.rem = 0;
		}
	}

	_polygon_insert_edge_into_its_y_bucket(polygon, e);

	e->x.rem -= dy; /* Bias the remainder for faster edge advancement. */
}

static void
active_list_reset(struct active_list *active)
{
	active->head = NULL;
	active->min_height = 0;
}

/*
 * Merge two sorted edge lists.
 * Input:
 *  - head_a: The head of the first list.
 *  - head_b: The head of the second list; head_b cannot be NULL.
 * Output:
 * Returns the head of the merged list.
 *
 * Implementation notes:
 * To make it fast (in particular, to reduce to an insertion sort whenever
 * one of the two input lists only has a single element) we iterate through
 * a list until its head becomes greater than the head of the other list,
 * then we switch their roles. As soon as one of the two lists is empty, we
 * just attach the other one to the current list and exit.
 * Writes to memory are only needed to "switch" lists (as it also requires
 * attaching to the output list the list which we will be iterating next) and
 * to attach the last non-empty list.
 */
static struct edge *
merge_sorted_edges(struct edge *head_a, struct edge *head_b)
{
	struct edge *head, **next;

	head = head_a;
	next = &head;

	while (1) {
		while (head_a != NULL && head_a->x.quo <= head_b->x.quo) {
			next = &head_a->next;
			head_a = head_a->next;
		}

		*next = head_b;
		if (head_a == NULL)
			return head;

		while (head_b != NULL && head_b->x.quo <= head_a->x.quo) {
			next = &head_b->next;
			head_b = head_b->next;
		}

		*next = head_a;
		if (head_b == NULL)
			return head;
	}
}

/*
 * Sort (part of) a list.
 * Input:
 *  - list: The list to be sorted; list cannot be NULL.
 *  - limit: Recursion limit.
 * Output:
 *  - head_out: The head of the sorted list containing the first 2^(level+1) elements of the
 *              input list; if the input list has fewer elements, head_out be a sorted list
 *              containing all the elements of the input list.
 * Returns the head of the list of unprocessed elements (NULL if the sorted list contains
 * all the elements of the input list).
 *
 * Implementation notes:
 * Special case single element list, unroll/inline the sorting of the first two elements.
 * Some tail recursion is used since we iterate on the bottom-up solution of the problem
 * (we start with a small sorted list and keep merging other lists of the same size to it).
 */
static struct edge *
sort_edges(struct edge  *list,
	   unsigned int  level,
	   struct edge **head_out)
{
	struct edge *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	/* Single element list -> return */
	if (head_other == NULL) {
		*head_out = list;
		return NULL;
	}

	/* Unroll the first iteration of the following loop (halves the number of calls to merge_sorted_edges):
	 *  - Initialize remaining to be the list containing the elements after the second in the input list.
	 *  - Initialize *head_out to be the sorted list containing the first two element.
	 */
	remaining = head_other->next;
	if (list->x.quo <= head_other->x.quo) {
		*head_out = list;
		/* list->next = head_other; */ /* The input list is already like this. */
		head_other->next = NULL;
	} else {
		*head_out = head_other;
		head_other->next = list;
		list->next = NULL;
	}

	for (i = 0; i < level && remaining; i++) {
		/* Extract a sorted list of the same size as *head_out
		 * (2^(i+1) elements) from the list of remaining elements. */
		remaining = sort_edges(remaining, i, &head_other);
		*head_out = merge_sorted_edges(*head_out, head_other);
	}

	/* *head_out now contains (at most) 2^(level+1) elements. */

	return remaining;
}

/* Test if the edges on the active list can be safely advanced by a
 * full row without intersections or any edges ending. */
inline static bool
active_list_can_step_full_row(struct active_list *active)
{
	const struct edge *e;
	int prev_x = INT_MIN;

	/* Recomputes the minimum height of all edges on the active
	 * list if we have been dropping edges. */
	if (active->min_height <= 0) {
		int min_height = INT_MAX;

		e = active->head;
		while (NULL != e) {
			if (e->height_left < min_height)
				min_height = e->height_left;
			e = e->next;
		}

		active->min_height = min_height;
	}

	if (active->min_height < FAST_SAMPLES_Y)
		return false;

	/* Check for intersections as no edges end during the next row. */
	e = active->head;
	while (NULL != e) {
		struct quorem x = e->x;

		if (!e->vertical) {
			x.quo += e->dxdy_full.quo;
			x.rem += e->dxdy_full.rem;
			if (x.rem >= 0)
				++x.quo;
		}

		if (x.quo <= prev_x)
			return false;

		prev_x = x.quo;
		e = e->next;
	}

	return true;
}

/* Merges edges on the given subpixel row from the polygon to the
 * active_list. */
inline static void
merge_edges(struct active_list *active,
	    grid_scaled_y_t y,
	    struct edge **ptail)
{
	/* Split off the edges on the current subrow and merge them into
	 * the active list. */
	int min_height = active->min_height;
	struct edge *subrow_edges = NULL;

	do {
		struct edge *tail = *ptail;
		if (NULL == tail)
			break;

		if (y == tail->ytop) {
			*ptail = tail->next;
			tail->next = subrow_edges;
			subrow_edges = tail;
			if (tail->height_left < min_height)
				min_height = tail->height_left;
		} else
			ptail = &tail->next;
	} while (1);

	if (subrow_edges) {
		sort_edges(subrow_edges, UINT_MAX, &subrow_edges);
		active->head = merge_sorted_edges(active->head, subrow_edges);
		active->min_height = min_height;
	}
}

/* Advance the edges on the active list by one subsample row by
 * updating their x positions.  Drop edges from the list that end. */
inline static void
substep_edges(struct active_list *active)
{
	struct edge **cursor = &active->head;
	grid_scaled_x_t prev_x = INT_MIN;
	struct edge *unsorted = NULL;

	do {
		struct edge *edge = *cursor;
		if (NULL == edge)
			break;

		if (0 != --edge->height_left) {
			edge->x.quo += edge->dxdy.quo;
			edge->x.rem += edge->dxdy.rem;
			if (edge->x.rem >= 0) {
				++edge->x.quo;
				edge->x.rem -= edge->dy;
			}

			if (edge->x.quo < prev_x) {
				*cursor = edge->next;
				edge->next = unsorted;
				unsorted = edge;
			} else {
				prev_x = edge->x.quo;
				cursor = &edge->next;
			}
		} else
			*cursor = edge->next;
	} while (1);

	if (unsorted) {
		sort_edges(unsorted, UINT_MAX, &unsorted);
		active->head = merge_sorted_edges(active->head, unsorted);
	}
}

inline static void
apply_nonzero_fill_rule_for_subrow(struct active_list *active,
				   struct cell_list *coverages)
{
	struct edge *edge = active->head;
	int winding = 0;
	int xstart;
	int xend;

	cell_list_rewind (coverages);

	while (NULL != edge) {
		xstart = edge->x.quo;
		winding = edge->dir;
		while (1) {
			edge = edge->next;
			if (NULL == edge)
				return cell_list_add_unbounded_subspan(coverages, xstart);

			winding += edge->dir;
			if (0 == winding) {
				if (edge->next == NULL ||
				    edge->next->x.quo != edge->x.quo)
					break;
			}
		}

		xend = edge->x.quo;
		cell_list_add_subspan(coverages, xstart, xend);

		edge = edge->next;
	}
}

static void
apply_nonzero_fill_rule_and_step_edges(struct active_list *active,
				       struct cell_list *coverages)
{
	struct edge **cursor = &active->head;
	struct edge *left_edge;

	left_edge = *cursor;
	while (NULL != left_edge) {
		struct edge *right_edge;
		int winding = left_edge->dir;

		left_edge->height_left -= FAST_SAMPLES_Y;
		if (left_edge->height_left)
			cursor = &left_edge->next;
		else
			*cursor = left_edge->next;

		do {
			right_edge = *cursor;
			if (NULL == right_edge)
				return cell_list_render_edge(coverages,
							     left_edge,
							     +1);

			right_edge->height_left -= FAST_SAMPLES_Y;
			if (right_edge->height_left)
				cursor = &right_edge->next;
			else
				*cursor = right_edge->next;

			winding += right_edge->dir;
			if (0 == winding) {
				if (right_edge->next == NULL ||
				    right_edge->next->x.quo != right_edge->x.quo)
					break;
			}

			if (!right_edge->vertical) {
				right_edge->x.quo += right_edge->dxdy_full.quo;
				right_edge->x.rem += right_edge->dxdy_full.rem;
				if (right_edge->x.rem >= 0) {
					++right_edge->x.quo;
					right_edge->x.rem -= right_edge->dy;
				}
			}
		} while (1);

		cell_list_render_edge(coverages, left_edge, +1);
		cell_list_render_edge(coverages, right_edge, -1);

		left_edge = *cursor;
	}
}

static void
tor_fini(struct tor *converter)
{
	polygon_fini(converter->polygon);
	cell_list_fini(converter->coverages);
}

static int
tor_init(struct tor *converter, const BoxRec *box, int num_edges)
{
	DBG(("%s: (%d, %d),(%d, %d) x (%d, %d)\n",
	     __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2,
	     FAST_SAMPLES_X, FAST_SAMPLES_Y));

	converter->xmin = box->x1;
	converter->ymin = box->y1;
	converter->xmax = box->x2;
	converter->ymax = box->y2;

	cell_list_init(converter->coverages);
	active_list_reset(converter->active);
	return polygon_init(converter->polygon,
			    num_edges,
			    box->y1 * FAST_SAMPLES_Y,
			    box->y2 * FAST_SAMPLES_Y);
}

static void
tor_add_edge(struct tor *converter,
	     int dx, int dy,
	     int top, int bottom,
	     const xLineFixed *edge,
	     int dir)
{
	int x1, x2;
	int y1, y2;

	y1 = dy + (edge->p1.y >> (16 - FAST_SAMPLES_Y_shift));
	y2 = dy + (edge->p2.y >> (16 - FAST_SAMPLES_Y_shift));
	if (y1 == y2)
		return;

	x1 = dx + (edge->p1.x >> (16 - FAST_SAMPLES_X_shift));
	x2 = dx + (edge->p2.x >> (16 - FAST_SAMPLES_X_shift));

	DBG(("%s: edge=(%d, %d), (%d, %d), top=%d, bottom=%d, dir=%d\n",
	     __FUNCTION__, x1, y1, x2, y2, top, bottom, dir));
	polygon_add_edge(converter->polygon,
			 x1, x2,
			 y1, y2,
			 top, bottom,
			 dir);
}

static bool
active_list_is_vertical(struct active_list *active)
{
	struct edge *e;

	for (e = active->head; e != NULL; e = e->next)
		if (!e->vertical)
			return false;

	return true;
}

static void
step_edges(struct active_list *active, int count)
{
	struct edge **cursor = &active->head;
	struct edge *edge;

	for (edge = *cursor; edge != NULL; edge = *cursor) {
		edge->height_left -= FAST_SAMPLES_Y * count;
		if (edge->height_left)
			cursor = &edge->next;
		else
			*cursor = edge->next;
	}
}

static void
tor_blt_span(struct sna *sna,
	     struct sna_composite_spans_op *op,
	     pixman_region16_t *clip,
	     const BoxRec *box,
	     int coverage)
{
	DBG(("%s: %d -> %d @ %d\n", __FUNCTION__, box->x1, box->x2, coverage));

	op->box(sna, op, box, AREA_TO_ALPHA(coverage));
	apply_damage_box(&op->base, box);
}

static void
tor_blt_span_clipped(struct sna *sna,
		     struct sna_composite_spans_op *op,
		     pixman_region16_t *clip,
		     const BoxRec *box,
		     int coverage)
{
	pixman_region16_t region;
	float opacity;

	opacity = AREA_TO_ALPHA(coverage);
	DBG(("%s: %d -> %d @ %f\n", __FUNCTION__, box->x1, box->x2, opacity));

	pixman_region_init_rects(&region, box, 1);
	RegionIntersect(&region, &region, clip);
	if (REGION_NUM_RECTS(&region)) {
		op->boxes(sna, op,
			  REGION_RECTS(&region),
			  REGION_NUM_RECTS(&region),
			  opacity);
		apply_damage(&op->base, &region);
	}
	pixman_region_fini(&region);
}

static void
tor_blt_span_mono(struct sna *sna,
		  struct sna_composite_spans_op *op,
		  pixman_region16_t *clip,
		  const BoxRec *box,
		  int coverage)
{
	if (coverage < FAST_SAMPLES_XY/2)
		return;

	tor_blt_span(sna, op, clip, box, FAST_SAMPLES_XY);
}

static void
tor_blt_span_mono_clipped(struct sna *sna,
			  struct sna_composite_spans_op *op,
			  pixman_region16_t *clip,
			  const BoxRec *box,
			  int coverage)
{
	if (coverage < FAST_SAMPLES_XY/2)
		return;

	tor_blt_span_clipped(sna, op, clip, box, FAST_SAMPLES_XY);
}

static void
tor_blt_span_mono_unbounded(struct sna *sna,
			    struct sna_composite_spans_op *op,
			    pixman_region16_t *clip,
			    const BoxRec *box,
			    int coverage)
{
	tor_blt_span(sna, op, clip, box,
		     coverage < FAST_SAMPLES_XY/2 ? 0 :FAST_SAMPLES_XY);
}

static void
tor_blt_span_mono_unbounded_clipped(struct sna *sna,
				    struct sna_composite_spans_op *op,
				    pixman_region16_t *clip,
				    const BoxRec *box,
				    int coverage)
{
	tor_blt_span_clipped(sna, op, clip, box,
			     coverage < FAST_SAMPLES_XY/2 ? 0 :FAST_SAMPLES_XY);
}

static void
tor_blt(struct sna *sna,
	struct sna_composite_spans_op *op,
	pixman_region16_t *clip,
	void (*span)(struct sna *sna,
		     struct sna_composite_spans_op *op,
		     pixman_region16_t *clip,
		     const BoxRec *box,
		     int coverage),
	struct cell_list *cells,
	int y, int height,
	int xmin, int xmax,
	int unbounded)
{
	struct cell *cell = cells->head;
	BoxRec box;
	int cover = 0;

	/* Skip cells to the left of the clip region. */
	while (cell != NULL && cell->x < xmin) {
		DBG(("%s: skipping cell (%d, %d, %d)\n",
		     __FUNCTION__,
		     cell->x, cell->covered_height, cell->uncovered_area));

		cover += cell->covered_height;
		cell = cell->next;
	}
	cover *= FAST_SAMPLES_X*2;

	box.y1 = y;
	box.y2 = y + height;
	box.x1 = xmin;

	/* Form the spans from the coverages and areas. */
	for (; cell != NULL; cell = cell->next) {
		int x = cell->x;

		DBG(("%s: cell=(%d, %d, %d), cover=%d, max=%d\n", __FUNCTION__,
		     cell->x, cell->covered_height, cell->uncovered_area,
		     cover, xmax));

		if (x >= xmax)
			break;

		box.x2 = x;
		if (box.x2 > box.x1 && (unbounded || cover))
			span(sna, op, clip, &box, cover);
		box.x1 = box.x2;

		cover += cell->covered_height*FAST_SAMPLES_X*2;

		if (cell->uncovered_area) {
			int area = cover - cell->uncovered_area;
			box.x2 = x + 1;
			if (unbounded || area)
				span(sna, op, clip, &box, area);
			box.x1 = box.x2;
		}
	}

	box.x2 = xmax;
	if (box.x2 > box.x1 && (unbounded || cover))
		span(sna, op, clip, &box, cover);
}

static void
tor_blt_empty(struct sna *sna,
	      struct sna_composite_spans_op *op,
	      pixman_region16_t *clip,
	      void (*span)(struct sna *sna,
			   struct sna_composite_spans_op *op,
			   pixman_region16_t *clip,
			   const BoxRec *box,
			   int coverage),
	      int y, int height,
	      int xmin, int xmax)
{
	BoxRec box;

	box.x1 = xmin;
	box.x2 = xmax;
	box.y1 = y;
	box.y2 = y + height;

	span(sna, op, clip, &box, 0);
}

static void
tor_render(struct sna *sna,
	   struct tor *converter,
	   struct sna_composite_spans_op *op,
	   pixman_region16_t *clip,
	   void (*span)(struct sna *sna,
			struct sna_composite_spans_op *op,
			pixman_region16_t *clip,
			const BoxRec *box,
			int coverage),
	   int unbounded)
{
	int ymin = converter->ymin;
	int xmin = converter->xmin;
	int xmax = converter->xmax;
	int i, j, h = converter->ymax - ymin;
	struct polygon *polygon = converter->polygon;
	struct cell_list *coverages = converter->coverages;
	struct active_list *active = converter->active;

	DBG(("%s: unbounded=%d\n", __FUNCTION__, unbounded));

	/* Render each pixel row. */
	for (i = 0; i < h; i = j) {
		int do_full_step = 0;

		j = i + 1;

		/* Determine if we can ignore this row or use the full pixel
		 * stepper. */
		if (!polygon->y_buckets[i]) {
			if (!active->head) {
				for (; j < h && !polygon->y_buckets[j]; j++)
					;
				DBG(("%s: no new edges and no exisiting edges, skipping, %d -> %d\n",
				     __FUNCTION__, i, j));

				if (unbounded)
					tor_blt_empty(sna, op, clip, span, i+ymin, j-i, xmin, xmax);
				continue;
			}

			do_full_step = active_list_can_step_full_row(active);
		}

		DBG(("%s: do_full_step=%d, new edges=%d\n",
		     __FUNCTION__, do_full_step, polygon->y_buckets[i] != NULL));
		if (do_full_step) {
			/* Step by a full pixel row's worth. */
			apply_nonzero_fill_rule_and_step_edges(active,
							       coverages);

			if (active_list_is_vertical(active)) {
				while (j < h &&
				       polygon->y_buckets[j] == NULL &&
				       active->min_height >= 2*FAST_SAMPLES_Y)
				{
					active->min_height -= FAST_SAMPLES_Y;
					j++;
				}
				if (j != i + 1)
					step_edges(active, j - (i + 1));

				DBG(("%s: vertical edges, full step (%d, %d)\n",
				    __FUNCTION__,  i, j));
			}
		} else {
			grid_scaled_y_t y = (i+ymin)*FAST_SAMPLES_Y;
			grid_scaled_y_t suby;

			/* Subsample this row. */
			for (suby = 0; suby < FAST_SAMPLES_Y; suby++) {
				if (polygon->y_buckets[i])
					merge_edges(active,
						    y + suby,
						    &polygon->y_buckets[i]);

				apply_nonzero_fill_rule_for_subrow(active,
								   coverages);
				substep_edges(active);
			}
		}

		if (coverages->head != &coverages->tail) {
			tor_blt(sna, op, clip, span, coverages,
				i+ymin, j-i, xmin, xmax,
				unbounded);
			cell_list_reset(coverages);
		} else if (unbounded)
			tor_blt_empty(sna, op, clip, span, i+ymin, j-i, xmin, xmax);

		if (!active->head)
			active->min_height = INT_MAX;
		else
			active->min_height -= FAST_SAMPLES_Y;
	}
}

static int operator_is_bounded(uint8_t op)
{
	switch (op) {
	case PictOpOver:
	case PictOpOutReverse:
	case PictOpAdd:
		return TRUE;
	default:
		return FALSE;
	}
}

static void
trapezoids_fallback(CARD8 op, PicturePtr src, PicturePtr dst,
		    PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
		    int ntrap, xTrapezoid * traps)
{
	ScreenPtr screen = dst->pDrawable->pScreen;

	if (maskFormat) {
		PixmapPtr scratch;
		PicturePtr mask;
		INT16 dst_x, dst_y;
		BoxRec bounds;
		int width, height, depth;
		pixman_image_t *image;
		pixman_format_code_t format;
		int error;

		dst_x = pixman_fixed_to_int(traps[0].left.p1.x);
		dst_y = pixman_fixed_to_int(traps[0].left.p1.y);

		miTrapezoidBounds(ntrap, traps, &bounds);
		if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
			return;

		DBG(("%s: bounds (%d, %d), (%d, %d)\n",
		     __FUNCTION__, bounds.x1, bounds.y1, bounds.x2, bounds.y2));

		if (!sna_compute_composite_extents(&bounds,
						   src, NULL, dst,
						   xSrc, ySrc,
						   0, 0,
						   bounds.x1, bounds.y1,
						   bounds.x2 - bounds.x1,
						   bounds.y2 - bounds.y1))
			return;

		DBG(("%s: extents (%d, %d), (%d, %d)\n",
		     __FUNCTION__, bounds.x1, bounds.y1, bounds.x2, bounds.y2));

		width  = bounds.x2 - bounds.x1;
		height = bounds.y2 - bounds.y1;
		bounds.x1 -= dst->pDrawable->x;
		bounds.y1 -= dst->pDrawable->y;
		depth = maskFormat->depth;
		format = maskFormat->format | (BitsPerPixel(depth) << 24);

		DBG(("%s: mask (%dx%d) depth=%d, format=%08x\n",
		     __FUNCTION__, width, height, depth, format));
		scratch = sna_pixmap_create_upload(screen,
						   width, height, depth);
		if (!scratch)
			return;

		memset(scratch->devPrivate.ptr, 0, scratch->devKind*height);
		image = pixman_image_create_bits(format, width, height,
						 scratch->devPrivate.ptr,
						 scratch->devKind);
		if (image) {
			for (; ntrap; ntrap--, traps++)
				pixman_rasterize_trapezoid(image,
							   (pixman_trapezoid_t *)traps,
							   -bounds.x1, -bounds.y1);

			pixman_image_unref(image);
		}

		mask = CreatePicture(0, &scratch->drawable,
				     PictureMatchFormat(screen, depth, format),
				     0, 0, serverClient, &error);
		screen->DestroyPixmap(scratch);
		if (!mask)
			return;

		CompositePicture(op, src, mask, dst,
				 xSrc + bounds.x1 - dst_x,
				 ySrc + bounds.y1 - dst_y,
				 0, 0,
				 bounds.x1, bounds.y1,
				 width, height);
		FreePicture(mask, 0);
	} else {
		if (dst->polyEdge == PolyEdgeSharp)
			maskFormat = PictureMatchFormat(screen, 1, PICT_a1);
		else
			maskFormat = PictureMatchFormat(screen, 8, PICT_a8);

		for (; ntrap; ntrap--, traps++)
			trapezoids_fallback(op,
					    src, dst, maskFormat,
					    xSrc, ySrc, 1, traps);
	}
}

static Bool
composite_aligned_boxes(CARD8 op,
			PicturePtr src,
			PicturePtr dst,
			PictFormatPtr maskFormat,
			INT16 src_x, INT16 src_y,
			int ntrap, xTrapezoid *traps)
{
	BoxRec stack_boxes[64], *boxes, extents;
	pixman_region16_t region, clip;
	struct sna *sna;
	struct sna_composite_op tmp;
	Bool ret = true;
	int dx, dy, n, num_boxes;

	if (NO_ALIGNED_BOXES)
		return false;

	DBG(("%s\n", __FUNCTION__));

	boxes = stack_boxes;
	if (ntrap > ARRAY_SIZE(stack_boxes))
		boxes = malloc(sizeof(BoxRec)*ntrap);

	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	extents.x1 = extents.y1 = 32767;
	extents.x2 = extents.y2 = -32767;
	num_boxes = 0;
	for (n = 0; n < ntrap; n++) {
		boxes[num_boxes].x1 = dx + pixman_fixed_to_int(traps[n].left.p1.x + pixman_fixed_1_minus_e/2);
		boxes[num_boxes].y1 = dy + pixman_fixed_to_int(traps[n].top + pixman_fixed_1_minus_e/2);
		boxes[num_boxes].x2 = dx + pixman_fixed_to_int(traps[n].right.p2.x + pixman_fixed_1_minus_e/2);
		boxes[num_boxes].y2 = dy + pixman_fixed_to_int(traps[n].bottom + pixman_fixed_1_minus_e/2);

		if (boxes[num_boxes].x1 >= boxes[num_boxes].x2)
			continue;
		if (boxes[num_boxes].y1 >= boxes[num_boxes].y2)
			continue;

		if (boxes[num_boxes].x1 < extents.x1)
			extents.x1 = boxes[num_boxes].x1;
		if (boxes[num_boxes].x2 > extents.x2)
			extents.x2 = boxes[num_boxes].x2;

		if (boxes[num_boxes].y1 < extents.y1)
			extents.y1 = boxes[num_boxes].y1;
		if (boxes[num_boxes].y2 > extents.y2)
			extents.y2 = boxes[num_boxes].y2;

		num_boxes++;
	}

	if (num_boxes == 0)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d) offset of (%d, %d)\n",
	     __FUNCTION__,
	     extents.x1, extents.y1,
	     extents.x2, extents.y2,
	     extents.x1 - boxes[0].x1,
	     extents.y1 - boxes[0].y1));

	src_x += extents.x1 - boxes[0].x1;
	src_y += extents.y1 - boxes[0].y1;

	if (!sna_compute_composite_region(&clip,
					  src, NULL, dst,
					  src_x,  src_y,
					  0, 0,
					  extents.x1 - dx, extents.y1 - dy,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: trapezoids do not intersect drawable clips\n",
		     __FUNCTION__)) ;
		goto done;
	}

	memset(&tmp, 0, sizeof(tmp));
	sna = to_sna_from_drawable(dst->pDrawable);
	if (!sna->render.composite(sna, op, src, NULL, dst,
				   src_x,  src_y,
				   0, 0,
				   extents.x1,  extents.y1,
				   extents.x2 - extents.x1,
				   extents.y2 - extents.y1,
				   &tmp)) {
		DBG(("%s: composite render op not supported\n",
		     __FUNCTION__));
		ret = false;
		goto done;
	}

	if (maskFormat ||
	    (op == PictOpSrc || op == PictOpClear) ||
	    num_boxes == 1) {
		pixman_region_init_rects(&region, boxes, num_boxes);
		RegionIntersect(&region, &region, &clip);
		if (REGION_NUM_RECTS(&region)) {
			tmp.boxes(sna, &tmp,
				  REGION_RECTS(&region),
				  REGION_NUM_RECTS(&region));
			apply_damage(&tmp, &region);
		}
		pixman_region_fini(&region);
	} else {
		for (n = 0; n < num_boxes; n++) {
			pixman_region_init_rects(&region, &boxes[n], 1);
			RegionIntersect(&region, &region, &clip);
			if (REGION_NUM_RECTS(&region)) {
				tmp.boxes(sna, &tmp,
					  REGION_RECTS(&region),
					  REGION_NUM_RECTS(&region));
				apply_damage(&tmp, &region);
			}
			pixman_region_fini(&region);
		}
	}
	tmp.done(sna, &tmp);

done:
	REGION_UNINIT(NULL, &clip);
	if (boxes != stack_boxes)
		free(boxes);

	return ret;
}

static inline int coverage(int samples, pixman_fixed_t f)
{
	return (samples * pixman_fixed_frac(f) + pixman_fixed_1/2) / pixman_fixed_1;
}

static void
composite_unaligned_box(struct sna *sna,
			struct sna_composite_spans_op *tmp,
			const BoxRec *box,
			float opacity,
			pixman_region16_t *clip)
{
	pixman_region16_t region;

	pixman_region_init_rects(&region, box, 1);
	RegionIntersect(&region, &region, clip);
	if (REGION_NUM_RECTS(&region)) {
		tmp->boxes(sna, tmp,
			  REGION_RECTS(&region),
			  REGION_NUM_RECTS(&region),
			  opacity);
		apply_damage(&tmp->base, &region);
	}
	pixman_region_fini(&region);
}

static void
composite_unaligned_trap_row(struct sna *sna,
			     struct sna_composite_spans_op *tmp,
			     xTrapezoid *trap, int dx,
			     int y1, int y2, int covered,
			     pixman_region16_t *clip)
{
	BoxRec box;
	int opacity;
	int x1, x2;

	if (covered == 0)
		return;

	if (y2 > clip->extents.y2)
		y2 = clip->extents.y2;
	if (y1 < clip->extents.y1)
		y1 = clip->extents.y1;
	if (y1 >= y2)
		return;

	x1 = dx + pixman_fixed_to_int(trap->left.p1.x);
	x2 = dx + pixman_fixed_to_int(trap->right.p1.x);
	if (x2 < clip->extents.x1 || x1 > clip->extents.x2)
		return;

	box.y1 = y1;
	box.y2 = y2;

	if (x1 == x2) {
		box.x1 = x1;
		box.x2 = x2 + 1;

		opacity = covered;
		opacity *= coverage(SAMPLES_X, trap->right.p1.x) - coverage(SAMPLES_X, trap->left.p1.x);

		if (opacity)
			composite_unaligned_box(sna, tmp, &box,
						opacity/255., clip);
	} else {
		if (pixman_fixed_frac(trap->left.p1.x)) {
			box.x1 = x1;
			box.x2 = x1++;

			opacity = covered;
			opacity *= SAMPLES_X - coverage(SAMPLES_X, trap->left.p1.x);

			if (opacity)
				composite_unaligned_box(sna, tmp, &box,
							opacity/255., clip);
		}

		if (x2 > x1) {
			box.x1 = x1;
			box.x2 = x2;

			composite_unaligned_box(sna, tmp, &box,
						covered*SAMPLES_X/255., clip);
		}

		if (pixman_fixed_frac(trap->right.p1.x)) {
			box.x1 = x2;
			box.x2 = x2 + 1;

			opacity = covered;
			opacity *= coverage(SAMPLES_X, trap->right.p1.x);

			if (opacity)
				composite_unaligned_box(sna, tmp, &box,
							opacity/255., clip);
		}
	}
}

static void
composite_unaligned_trap(struct sna *sna,
			struct sna_composite_spans_op *tmp,
			xTrapezoid *trap,
			int dx, int dy,
			pixman_region16_t *clip)
{
	int y1, y2;

	y1 = dy + pixman_fixed_to_int(trap->top);
	y2 = dy + pixman_fixed_to_int(trap->bottom);

	if (y1 == y2) {
		composite_unaligned_trap_row(sna, tmp, trap, dx,
					     y1, y1 + 1,
					     coverage(SAMPLES_Y, trap->bottom) - coverage(SAMPLES_Y, trap->top),
					     clip);
	} else {
		if (pixman_fixed_frac(trap->top)) {
			composite_unaligned_trap_row(sna, tmp, trap, dx,
						     y1, y1 + 1,
						     SAMPLES_Y - coverage(SAMPLES_Y, trap->top),
						     clip);
			y1++;
		}

		if (y2 > y1)
			composite_unaligned_trap_row(sna, tmp, trap, dx,
						     y1, y2,
						     SAMPLES_Y,
						     clip);

		if (pixman_fixed_frac(trap->bottom))
			composite_unaligned_trap_row(sna, tmp, trap, dx,
						     y2, y2 + 1,
						     coverage(SAMPLES_Y, trap->bottom),
						     clip);
	}
}

inline static void
blt_opacity(PixmapPtr scratch,
	    int x1, int x2,
	    int y, int h,
	    uint8_t opacity)
{
	uint8_t *ptr;

	if (opacity == 0xff)
		return;

	if (x1 < 0)
		x1 = 0;
	if (x2 > scratch->drawable.width)
		x2 = scratch->drawable.width;
	if (x1 >= x2)
		return;

	x2 -= x1;

	ptr = scratch->devPrivate.ptr;
	ptr += scratch->devKind * y;
	ptr += x1;
	do {
		if (x2 == 1)
			*ptr = opacity;
		else
			memset(ptr, opacity, x2);
		ptr += scratch->devKind;
	} while (--h);
}

static void
blt_unaligned_box_row(PixmapPtr scratch,
		      BoxPtr extents,
		      xTrapezoid *trap,
		      int y1, int y2,
		      int covered)
{
	int x1, x2;

	if (y2 > scratch->drawable.height)
		y2 = scratch->drawable.height;
	if (y1 < 0)
		y1 = 0;
	if (y1 >= y2)
		return;

	y2 -= y1;

	x1 = pixman_fixed_to_int(trap->left.p1.x);
	x2 = pixman_fixed_to_int(trap->right.p1.x);

	x1 -= extents->x1;
	x2 -= extents->x1;

	if (x1 == x2) {
		blt_opacity(scratch,
			    x1, x1+1,
			    y1, y2,
			    covered * (coverage(SAMPLES_X, trap->right.p1.x) - coverage(SAMPLES_X, trap->left.p1.x)));
	} else {
		if (pixman_fixed_frac(trap->left.p1.x))
			blt_opacity(scratch,
				    x1, x1+1,
				    y1, y2,
				    covered * (SAMPLES_X - coverage(SAMPLES_X, trap->left.p1.x)));

		if (x2 > x1 + 1) {
			blt_opacity(scratch,
				    x1 + 1, x2,
				    y1, y2,
				    covered*SAMPLES_X);
		}

		if (pixman_fixed_frac(trap->right.p1.x))
			blt_opacity(scratch,
				    x2, x2 + 1,
				    y1, y2,
				    covered * coverage(SAMPLES_X, trap->right.p1.x));
	}
}

static Bool
composite_unaligned_boxes_fallback(CARD8 op,
				   PicturePtr src,
				   PicturePtr dst,
				   INT16 src_x, INT16 src_y,
				   int ntrap, xTrapezoid *traps)
{
	ScreenPtr screen = dst->pDrawable->pScreen;
	INT16 dst_x = pixman_fixed_to_int(traps[0].left.p1.x);
	INT16 dst_y = pixman_fixed_to_int(traps[0].left.p1.y);
	int dx = dst->pDrawable->x;
	int dy = dst->pDrawable->y;
	int n;

	for (n = 0; n < ntrap; n++) {
		xTrapezoid *t = &traps[n];
		PixmapPtr scratch;
		PicturePtr mask;
		BoxRec extents;
		int error;
		int y1, y2;

		extents.x1 = pixman_fixed_to_int(t->left.p1.x);
		extents.x2 = pixman_fixed_to_int(t->right.p1.x + pixman_fixed_1_minus_e);
		extents.y1 = pixman_fixed_to_int(t->top);
		extents.y2 = pixman_fixed_to_int(t->bottom + pixman_fixed_1_minus_e);

		if (!sna_compute_composite_extents(&extents,
						   src, NULL, dst,
						   src_x, src_y,
						   0, 0,
						   extents.x1, extents.y1,
						   extents.x2 - extents.x1,
						   extents.y2 - extents.y1))
			continue;

		scratch = sna_pixmap_create_upload(screen,
						   extents.x2 - extents.x1,
						   extents.y2 - extents.y1,
						   8);
		if (!scratch)
			continue;

		memset(scratch->devPrivate.ptr, 0xff,
		       scratch->devKind * (extents.y2 - extents.y1));

		extents.x1 -= dx;
		extents.x2 -= dx;
		extents.y1 -= dy;
		extents.y2 -= dy;

		y1 = pixman_fixed_to_int(t->top) - extents.y1;
		y2 = pixman_fixed_to_int(t->bottom) - extents.y1;

		if (y1 == y2) {
			blt_unaligned_box_row(scratch, &extents, t, y1, y1 + 1,
					      coverage(SAMPLES_Y, t->bottom) - coverage(SAMPLES_Y, t->top));
		} else {
			if (pixman_fixed_frac(t->top))
				blt_unaligned_box_row(scratch, &extents, t, y1, y1 + 1,
						      SAMPLES_Y - coverage(SAMPLES_Y, t->top));

			if (y2 > y1 + 1)
				blt_unaligned_box_row(scratch, &extents, t, y1+1, y2,
						      SAMPLES_Y);

			if (pixman_fixed_frac(t->bottom))
				blt_unaligned_box_row(scratch, &extents, t, y2, y2+1,
						      coverage(SAMPLES_Y, t->bottom));
		}

		mask = CreatePicture(0, &scratch->drawable,
				     PictureMatchFormat(screen, 8, PICT_a8),
				     0, 0, serverClient, &error);
		screen->DestroyPixmap(scratch);
		if (mask) {
			CompositePicture(op, src, mask, dst,
					 src_x + extents.x1 - dst_x,
					 src_y + extents.y1 - dst_y,
					 0, 0,
					 extents.x1, extents.y1,
					 extents.x2 - extents.x1,
					 extents.y2 - extents.y1);
			FreePicture(mask, 0);
		}
	}

	return TRUE;
}

static Bool
composite_unaligned_boxes(CARD8 op,
			  PicturePtr src,
			  PicturePtr dst,
			  PictFormatPtr maskFormat,
			  INT16 src_x, INT16 src_y,
			  int ntrap, xTrapezoid *traps)
{
	struct sna *sna;
	BoxRec extents;
	struct sna_composite_spans_op tmp;
	pixman_region16_t clip;
	int dst_x, dst_y;
	int dx, dy, n;

	if (NO_UNALIGNED_BOXES)
		return false;

	DBG(("%s\n", __FUNCTION__));

	/* need a span converter to handle overlapping traps */
	if (ntrap > 1 && maskFormat)
		return false;

	sna = to_sna_from_drawable(dst->pDrawable);
	if (!sna->render.composite_spans)
		return composite_unaligned_boxes_fallback(op, src, dst, src_x, src_y, ntrap, traps);

	dst_x = extents.x1 = pixman_fixed_to_int(traps[0].left.p1.x);
	extents.x2 = pixman_fixed_to_int(traps[0].right.p1.x + pixman_fixed_1_minus_e);
	dst_y = extents.y1 = pixman_fixed_to_int(traps[0].top);
	extents.y2 = pixman_fixed_to_int(traps[0].bottom + pixman_fixed_1_minus_e);

	DBG(("%s: src=(%d, %d), dst=(%d, %d)\n",
	     __FUNCTION__, src_x, src_y, dst_x, dst_y));

	for (n = 1; n < ntrap; n++) {
		int x1 = pixman_fixed_to_int(traps[n].left.p1.x);
		int x2 = pixman_fixed_to_int(traps[n].right.p1.x + pixman_fixed_1_minus_e);
		int y1 = pixman_fixed_to_int(traps[n].top);
		int y2 = pixman_fixed_to_int(traps[n].bottom + pixman_fixed_1_minus_e);

		if (x1 < extents.x1)
			extents.x1 = x1;
		if (x2 > extents.x2)
			extents.x2 = x2;
		if (y1 < extents.y1)
			extents.y1 = y1;
		if (y2 > extents.y2)
			extents.y2 = y2;
	}

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	if (!sna_compute_composite_region(&clip,
					  src, NULL, dst,
					  src_x + extents.x1 - dst_x,
					  src_y + extents.y1 - dst_y,
					  0, 0,
					  extents.x1, extents.y1,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: trapezoids do not intersect drawable clips\n",
		     __FUNCTION__)) ;
		return true;
	}

	extents = *RegionExtents(&clip);
	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d) src -> (%d, %d)\n",
	     __FUNCTION__,
	     extents.x1, extents.y1,
	     extents.x2, extents.y2,
	     dx, dy,
	     src_x + extents.x1 - dst_x - dx,
	     src_y + extents.y1 - dst_y - dy));

	memset(&tmp, 0, sizeof(tmp));
	if (!sna->render.composite_spans(sna, op, src, dst,
					 src_x + extents.x1 - dst_x - dx,
					 src_y + extents.y1 - dst_y - dy,
					 extents.x1,  extents.y1,
					 extents.x2 - extents.x1,
					 extents.y2 - extents.y1,
					 &tmp)) {
		DBG(("%s: composite spans render op not supported\n",
		     __FUNCTION__));
		return false;
	}

	for (n = 0; n < ntrap; n++)
		composite_unaligned_trap(sna, &tmp, &traps[n], dx, dy, &clip);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &clip);
	return true;
}

static bool
tor_scan_converter(CARD8 op, PicturePtr src, PicturePtr dst,
		   PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
		   int ntrap, xTrapezoid *traps)
{
	struct sna *sna;
	struct sna_composite_spans_op tmp;
	struct tor tor;
	void (*span)(struct sna *sna,
		     struct sna_composite_spans_op *op,
		     pixman_region16_t *clip,
		     const BoxRec *box,
		     int coverage);
	BoxRec extents;
	pixman_region16_t clip;
	int16_t dst_x, dst_y;
	int16_t dx, dy;
	int n;

	if (NO_SCAN_CONVERTER)
		return false;

	/* XXX strict adhernce to the Reneder specification */
	if (dst->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	sna = to_sna_from_drawable(dst->pDrawable);
	if (!sna->render.composite_spans) {
		DBG(("%s: fallback -- composite spans not supported\n",
		     __FUNCTION__));
		return false;
	}

	dst_x = pixman_fixed_to_int(traps[0].left.p1.x);
	dst_y = pixman_fixed_to_int(traps[0].left.p1.y);

	miTrapezoidBounds(ntrap, traps, &extents);
	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (!sna_compute_composite_region(&clip,
					  src, NULL, dst,
					  src_x + extents.x1 - dst_x,
					  src_y + extents.y1 - dst_y,
					  0, 0,
					  extents.x1, extents.y1,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: trapezoids do not intersect drawable clips\n",
		     __FUNCTION__)) ;
		return true;
	}

	extents = *RegionExtents(&clip);
	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d) src -> (%d, %d)\n",
	     __FUNCTION__,
	     extents.x1, extents.y1,
	     extents.x2, extents.y2,
	     dx, dy,
	     src_x + extents.x1 - dst_x - dx,
	     src_y + extents.y1 - dst_y - dy));

	memset(&tmp, 0, sizeof(tmp));
	if (!sna->render.composite_spans(sna, op, src, dst,
					 src_x + extents.x1 - dst_x - dx,
					 src_y + extents.y1 - dst_y - dy,
					 extents.x1,  extents.y1,
					 extents.x2 - extents.x1,
					 extents.y2 - extents.y1,
					 &tmp)) {
		DBG(("%s: fallback -- composite spans render op not supported\n",
		     __FUNCTION__));
		return false;
	}

	dx *= FAST_SAMPLES_X;
	dy *= FAST_SAMPLES_Y;
	if (tor_init(&tor, &extents, 2*ntrap))
		goto skip;

	for (n = 0; n < ntrap; n++) {
		int top, bottom;

		if (!xTrapezoidValid(&traps[n]))
			continue;

		if (pixman_fixed_to_int(traps[n].top) + dst->pDrawable->y >= extents.y2 ||
		    pixman_fixed_to_int(traps[n].bottom) + dst->pDrawable->y < extents.y1)
			continue;

		top = dy + (traps[n].top >> (16 - FAST_SAMPLES_Y_shift));
		bottom = dy + (traps[n].bottom >> (16 - FAST_SAMPLES_Y_shift));
		if (top >= bottom)
			continue;

		tor_add_edge(&tor, dx, dy, top, bottom, &traps[n].left, 1);
		tor_add_edge(&tor, dx, dy, top, bottom, &traps[n].right, -1);
	}

	if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp) {
		/* XXX An imprecise approximation */
		if (maskFormat && !operator_is_bounded(op)) {
			span = tor_blt_span_mono_unbounded;
			if (REGION_NUM_RECTS(&clip) > 1)
				span = tor_blt_span_mono_unbounded_clipped;
		} else {
			span = tor_blt_span_mono;
			if (REGION_NUM_RECTS(&clip) > 1)
				span = tor_blt_span_mono_clipped;
		}
	} else {
		span = tor_blt_span;
		if (REGION_NUM_RECTS(&clip) > 1)
			span = tor_blt_span_clipped;
	}

	tor_render(sna, &tor, &tmp, &clip, span,
		   maskFormat && !operator_is_bounded(op));

skip:
	tor_fini(&tor);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &clip);
	return true;
}

void
sna_composite_trapezoids(CARD8 op,
			 PicturePtr src,
			 PicturePtr dst,
			 PictFormatPtr maskFormat,
			 INT16 xSrc, INT16 ySrc,
			 int ntrap, xTrapezoid *traps)
{
	struct sna *sna = to_sna_from_drawable(dst->pDrawable);
	bool rectilinear = true;
	bool pixel_aligned = true;
	int n;

	DBG(("%s(op=%d, src=(%d, %d), mask=%08x, ntrap=%d)\n", __FUNCTION__,
	     op, xSrc, ySrc,
	     maskFormat ? (int)maskFormat->format : 0,
	     ntrap));

	if (ntrap == 0)
		return;

	if (NO_ACCEL)
		goto fallback;

	if (sna->kgem.wedged || !sna->have_render) {
		DBG(("%s: fallback -- wedged=%d, have_render=%d\n",
		     __FUNCTION__, sna->kgem.wedged, sna->have_render));
		goto fallback;
	}

	if (dst->alphaMap || src->alphaMap) {
		DBG(("%s: fallback -- alpha maps=(dst=%p, src=%p)\n",
		     __FUNCTION__, dst->alphaMap, src->alphaMap));
		goto fallback;
	}

	if (too_small(sna, dst->pDrawable) && !picture_is_gpu(src)) {
		DBG(("%s: fallback -- dst is too small, %dx%d\n",
		     __FUNCTION__,
		     dst->pDrawable->width,
		     dst->pDrawable->height));
		goto fallback;
	}

	/* scan through for fast rectangles */
	for (n = 0; n < ntrap && rectilinear; n++) {
		rectilinear &=
			traps[n].left.p1.x == traps[n].left.p2.x &&
			traps[n].right.p1.x == traps[n].right.p2.x;
		pixel_aligned &=
			((traps[n].top | traps[n].bottom |
			  traps[n].left.p1.x | traps[n].left.p2.x |
			  traps[n].right.p1.x | traps[n].right.p2.x)
			 & pixman_fixed_1_minus_e) == 0;
	}

	if (rectilinear) {
		pixel_aligned |= maskFormat ?
			maskFormat->depth == 1 :
			dst->polyEdge == PolyEdgeSharp;
		if (pixel_aligned) {
			if (composite_aligned_boxes(op, src, dst,
						    maskFormat,
						    xSrc, ySrc,
						    ntrap, traps))
			    return;
		} else {
			if (composite_unaligned_boxes(op, src, dst,
						      maskFormat,
						      xSrc, ySrc,
						      ntrap, traps))
				return;
		}
	}

	if (tor_scan_converter(op, src, dst, maskFormat,
			       xSrc, ySrc, ntrap, traps))
		return;

fallback:
	DBG(("%s: fallback mask=%08x, ntrap=%d\n", __FUNCTION__,
	     maskFormat ? (unsigned)maskFormat->format : 0, ntrap));
	trapezoids_fallback(op, src, dst, maskFormat,
			    xSrc, ySrc,
			    ntrap, traps);
}
