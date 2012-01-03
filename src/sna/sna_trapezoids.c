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

#include <fb.h>
#include <mipict.h>
#include <fbpict.h>

#if DEBUG_TRAPEZOIDS
#undef DBG
#define DBG(x) ErrorF x
#endif

#define NO_ACCEL 0
#define NO_ALIGNED_BOXES 0
#define NO_UNALIGNED_BOXES 0
#define NO_SCAN_CONVERTER 0

/* TODO: Emit unantialiased and MSAA triangles. */

#ifndef MAX
#define MAX(x,y) ((x) >= (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x,y) ((x) <= (y) ? (x) : (y))
#endif

#define SAMPLES_X 17
#define SAMPLES_Y 15

#define FAST_SAMPLES_shift 2
#define FAST_SAMPLES_X (1<<FAST_SAMPLES_shift)
#define FAST_SAMPLES_Y (1<<FAST_SAMPLES_shift)
#define FAST_SAMPLES_mask ((1<<FAST_SAMPLES_shift)-1)

typedef void (*span_func_t)(struct sna *sna,
			    struct sna_composite_spans_op *op,
			    pixman_region16_t *clip,
			    const BoxRec *box,
			    int coverage);

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

static void _apply_damage_box(struct sna_composite_op *op, const BoxRec *box)
{
	BoxRec r;

	r.x1 = box->x1 + op->dst.x;
	r.x2 = box->x2 + op->dst.x;
	r.y1 = box->y1 + op->dst.y;
	r.y2 = box->y2 + op->dst.y;

	assert_pixmap_contains_box(op->dst.pixmap, &r);
	sna_damage_add_box(op->damage, &r);
}

inline static void apply_damage_box(struct sna_composite_op *op, const BoxRec *box)
{
	if (op->damage)
		_apply_damage_box(op, box);
}

typedef int grid_scaled_x_t;
typedef int grid_scaled_y_t;

#define FAST_SAMPLES_X_TO_INT_FRAC(x, i, f) \
	_GRID_TO_INT_FRAC_shift(x, i, f, FAST_SAMPLES_shift)

#define FAST_SAMPLES_INT(x) ((x) >> (FAST_SAMPLES_shift))
#define FAST_SAMPLES_FRAC(x) ((x) & (FAST_SAMPLES_mask))

#define _GRID_TO_INT_FRAC_shift(t, i, f, b) do {	\
    (f) = FAST_SAMPLES_FRAC(t);				\
    (i) = FAST_SAMPLES_INT(t);				\
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

struct edge {
	struct edge *next, *prev;

	int dir;
	int vertical;

	grid_scaled_y_t height_left;

	/* Current x coordinate while the edge is on the active
	 * list. Initialised to the x coordinate of the top of the
	 * edge. The quotient is in grid_scaled_x_t units and the
	 * remainder is mod dy in grid_scaled_y_t units.*/
	struct quorem x;

	/* Advance of the current x when moving down a subsample line. */
	struct quorem dxdy;
	grid_scaled_y_t dy;

	/* The clipped y of the top of the edge. */
	grid_scaled_y_t ytop;

	/* y2-y1 after orienting the edge downwards.  */
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
	struct cell head, tail;

	struct cell *cursor;

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
	struct edge head, tail;

	/* A lower bound on the height of the active edges is used to
	 * estimate how soon some active edge ends.	 We can't advance the
	 * scan conversion by a full pixel row if an edge ends somewhere
	 * within it. */
	grid_scaled_y_t min_height;
	int is_vertical;
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
	if (qr.rem && (a^b)<0) {
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
	if (qr.rem && (xa>=0) != (b>=0)) {
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
	struct cell *tail = cells->cursor;
	if (tail->x > x)
		cell_list_rewind (cells);
}

static void
cell_list_init(struct cell_list *cells)
{
	pool_init(cells->cell_pool.base,
		  256*sizeof(struct cell),
		  sizeof(cells->cell_pool.embedded));
	cells->tail.next = NULL;
	cells->tail.x = INT_MAX;
	cells->head.x = INT_MIN;
	cells->head.next = &cells->tail;
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
	cells->head.next = &cells->tail;
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

	cell->next = tail->next;
	tail->next = cell;
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
	struct cell *tail = cells->cursor;

	if (tail->x == x)
		return tail;

	do {
		if (tail->next->x > x)
			break;

		tail = tail->next;
	} while (1);

	if (tail->x != x)
		tail = cell_list_alloc (cells, tail, x);

	return cells->cursor = tail;
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

static void
cell_list_render_edge(struct cell_list *cells, struct edge *edge, int sign)
{
	struct cell *cell;
	grid_scaled_x_t fx;
	int ix;

	FAST_SAMPLES_X_TO_INT_FRAC(edge->x.quo, ix, fx);

	/* We always know that ix1 is >= the cell list cursor in this
	 * case due to the no-intersections precondition.  */
	cell = cell_list_find(cells, ix);
	cell->covered_height += sign*FAST_SAMPLES_Y;
	cell->uncovered_area += sign*2*fx*FAST_SAMPLES_Y;
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
	unsigned num_buckets =
		EDGE_Y_BUCKET_INDEX(ymax+EDGE_Y_BUCKET_HEIGHT-1, ymin);

	if (unlikely(h > 0x7FFFFFFFU - EDGE_Y_BUCKET_HEIGHT))
		goto bail_no_mem; /* even if you could, you wouldn't want to. */

	polygon->edges = polygon->edges_embedded;
	polygon->y_buckets = polygon->y_buckets_embedded;

	polygon->num_edges = 0;
	if (num_edges > (int)ARRAY_SIZE(polygon->edges_embedded)) {
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

	DBG(("%s: edge=(%d [%d.%d], %d [%d.%d]), (%d [%d.%d], %d [%d.%d]), top=%d [%d.%d], bottom=%d [%d.%d], dir=%d\n",
	     __FUNCTION__,
	     x1, FAST_SAMPLES_INT(x1), FAST_SAMPLES_FRAC(x1),
	     y1, FAST_SAMPLES_INT(y1), FAST_SAMPLES_FRAC(y1),
	     x2, FAST_SAMPLES_INT(x2), FAST_SAMPLES_FRAC(x2),
	     y2, FAST_SAMPLES_INT(y2), FAST_SAMPLES_FRAC(y2),
	     top, FAST_SAMPLES_INT(top), FAST_SAMPLES_FRAC(top),
	     bottom, FAST_SAMPLES_INT(bottom), FAST_SAMPLES_FRAC(bottom),
	     dir));
	assert (dy > 0);

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
	}

	_polygon_insert_edge_into_its_y_bucket(polygon, e);

	e->x.rem -= dy; /* Bias the remainder for faster edge advancement. */
}

inline static void
polygon_add_line(struct polygon *polygon,
		 const xPointFixed *p1,
		 const xPointFixed *p2)
{
	struct edge *e = &polygon->edges[polygon->num_edges];
	grid_scaled_x_t dx = p2->x - p1->x;
	grid_scaled_y_t dy = p2->y - p1->y;
	grid_scaled_y_t top, bot;

	if (dy == 0)
		return;

	DBG(("%s: line=(%d, %d), (%d, %d)\n",
	     __FUNCTION__, (int)p1->x, (int)p1->y, (int)p2->x, (int)p2->y));

	e->dir = 1;
	if (dy < 0) {
		const xPointFixed *t;

		dx = -dx;
		dy = -dy;

		e->dir = -1;

		t = p1;
		p1 = p2;
		p2 = t;
	}
	assert (dy > 0);
	e->dy = dy;

	top = MAX(p1->y, polygon->ymin);
	bot = MIN(p2->y, polygon->ymax);
	if (bot <= top)
		return;

	e->ytop = top;
	e->height_left = bot - top;

	if (dx == 0) {
		e->vertical = true;
		e->x.quo = p1->x;
		e->x.rem = 0;
		e->dxdy.quo = 0;
		e->dxdy.rem = 0;
	} else {
		e->vertical = false;
		e->dxdy = floored_divrem(dx, dy);
		if (top == p1->y) {
			e->x.quo = p1->x;
			e->x.rem = -dy;
		} else {
			e->x = floored_muldivrem(top - p1->y, dx, dy);
			e->x.quo += p1->x;
			e->x.rem -= dy;
		}
	}

	if (polygon->num_edges > 0) {
		struct edge *prev = &polygon->edges[polygon->num_edges-1];
		/* detect degenerate triangles inserted into tristrips */
		if (e->dir == -prev->dir &&
		    e->ytop == prev->ytop &&
		    e->height_left == prev->height_left &&
		    e->x.quo == prev->x.quo &&
		    e->x.rem == prev->x.rem &&
		    e->dxdy.quo == prev->dxdy.quo &&
		    e->dxdy.rem == prev->dxdy.rem) {
			unsigned ix = EDGE_Y_BUCKET_INDEX(e->ytop,
							  polygon->ymin);
			polygon->y_buckets[ix] = prev->next;
			polygon->num_edges--;
			return;
		}
	}

	_polygon_insert_edge_into_its_y_bucket(polygon, e);
	polygon->num_edges++;
}

static void
active_list_reset(struct active_list *active)
{
	active->head.vertical = 1;
	active->head.height_left = INT_MAX;
	active->head.x.quo = INT_MIN;
	active->head.prev = NULL;
	active->head.next = &active->tail;
	active->tail.prev = &active->head;
	active->tail.next = NULL;
	active->tail.x.quo = INT_MAX;
	active->tail.height_left = INT_MAX;
	active->tail.vertical = 1;
	active->min_height = INT_MAX;
	active->is_vertical = 1;
}

static struct edge *
merge_sorted_edges(struct edge *head_a, struct edge *head_b)
{
	struct edge *head, **next, *prev;
	int32_t x;

	prev = head_a->prev;
	next = &head;
	if (head_a->x.quo <= head_b->x.quo) {
		head = head_a;
	} else {
		head = head_b;
		head_b->prev = prev;
		goto start_with_b;
	}

	do {
		x = head_b->x.quo;
		while (head_a != NULL && head_a->x.quo <= x) {
			prev = head_a;
			next = &head_a->next;
			head_a = head_a->next;
		}

		head_b->prev = prev;
		*next = head_b;
		if (head_a == NULL)
			return head;

start_with_b:
		x = head_a->x.quo;
		while (head_b != NULL && head_b->x.quo <= x) {
			prev = head_b;
			next = &head_b->next;
			head_b = head_b->next;
		}

		head_a->prev = prev;
		*next = head_a;
		if (head_b == NULL)
			return head;
	} while (1);
}

static struct edge *
sort_edges(struct edge  *list,
	   unsigned int  level,
	   struct edge **head_out)
{
	struct edge *head_other, *remaining;
	unsigned int i;

	head_other = list->next;
	if (head_other == NULL) {
		*head_out = list;
		return NULL;
	}

	remaining = head_other->next;
	if (list->x.quo <= head_other->x.quo) {
		*head_out = list;
		head_other->next = NULL;
	} else {
		*head_out = head_other;
		head_other->prev = list->prev;
		head_other->next = list;
		list->prev = head_other;
		list->next = NULL;
	}

	for (i = 0; i < level && remaining; i++) {
		remaining = sort_edges(remaining, i, &head_other);
		*head_out = merge_sorted_edges(*head_out, head_other);
	}

	return remaining;
}

static struct edge *
merge_unsorted_edges (struct edge *head, struct edge *unsorted)
{
	sort_edges (unsorted, UINT_MAX, &unsorted);
	return merge_sorted_edges (head, unsorted);
}

/* Test if the edges on the active list can be safely advanced by a
 * full row without intersections or any edges ending. */
inline static bool
can_full_step(struct active_list *active)
{
	const struct edge *e;

	/* Recomputes the minimum height of all edges on the active
	 * list if we have been dropping edges. */
	if (active->min_height <= 0) {
		int min_height = INT_MAX;
		int is_vertical = 1;

		for (e = active->head.next; &active->tail != e; e = e->next) {
			if (e->height_left < min_height)
				min_height = e->height_left;
			is_vertical &= e->vertical;
		}

		active->is_vertical = is_vertical;
		active->min_height = min_height;
	}

	if (active->min_height < FAST_SAMPLES_Y)
		return false;

	return active->is_vertical;
}

inline static void
merge_edges(struct active_list *active, struct edge *edges)
{
	active->head.next = merge_unsorted_edges (active->head.next, edges);
}

inline static void
fill_buckets(struct active_list *active,
	     struct edge *edge,
	     struct edge **buckets)
{
	int min_height = active->min_height;
	int is_vertical = active->is_vertical;

	while (edge) {
		struct edge *next = edge->next;
		struct edge **b = &buckets[edge->ytop & (FAST_SAMPLES_Y-1)];
		if (*b)
			(*b)->prev = edge;
		edge->next = *b;
		edge->prev = NULL;
		*b = edge;
		if (edge->height_left < min_height)
			min_height = edge->height_left;
		is_vertical &= edge->vertical;
		edge = next;
	}

	active->is_vertical = is_vertical;
	active->min_height = min_height;
}

inline static void
nonzero_subrow(struct active_list *active, struct cell_list *coverages)
{
	struct edge *edge = active->head.next;
	grid_scaled_x_t prev_x = INT_MIN;
	int winding = 0, xstart = INT_MIN;

	cell_list_rewind (coverages);

	while (&active->tail != edge) {
		struct edge *next = edge->next;

		winding += edge->dir;
		if (0 == winding) {
			if (edge->next->x.quo != edge->x.quo) {
				cell_list_add_subspan(coverages,
						      xstart, edge->x.quo);
				xstart = INT_MIN;
			}
		} else if (xstart < 0)
			xstart = edge->x.quo;

		if (--edge->height_left) {
			if (!edge->vertical) {
				edge->x.quo += edge->dxdy.quo;
				edge->x.rem += edge->dxdy.rem;
				if (edge->x.rem >= 0) {
					++edge->x.quo;
					edge->x.rem -= edge->dy;
				}
			}

			if (edge->x.quo < prev_x) {
				struct edge *pos = edge->prev;
				pos->next = next;
				next->prev = pos;
				do {
					pos = pos->prev;
				} while (edge->x.quo < pos->x.quo);
				pos->next->prev = edge;
				edge->next = pos->next;
				edge->prev = pos;
				pos->next = edge;
			} else
				prev_x = edge->x.quo;
		} else {
			edge->prev->next = next;
			next->prev = edge->prev;
		}

		edge = next;
	}
}

static void
nonzero_row(struct active_list *active, struct cell_list *coverages)
{
	struct edge *left = active->head.next;

	assert(active->is_vertical);

	while (&active->tail != left) {
		struct edge *right;
		int winding = left->dir;

		left->height_left -= FAST_SAMPLES_Y;
		if (! left->height_left) {
			left->prev->next = left->next;
			left->next->prev = left->prev;
		}

		right = left->next;
		do {
			right->height_left -= FAST_SAMPLES_Y;
			if (!right->height_left) {
				right->prev->next = right->next;
				right->next->prev = right->prev;
			}

			winding += right->dir;
			if (0 == winding)
				break;

			right = right->next;
		} while (1);

		cell_list_render_edge(coverages, left, +1);
		cell_list_render_edge(coverages, right, -1);

		left = right->next;
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
	DBG(("%s: (%d, %d),(%d, %d) x (%d, %d), num_edges=%d\n",
	     __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2,
	     FAST_SAMPLES_X, FAST_SAMPLES_Y,
	     num_edges));

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
	     const xTrapezoid *t,
	     const xLineFixed *edge,
	     int dir)
{
	polygon_add_edge(converter->polygon,
			 edge->p1.x, edge->p2.x,
			 edge->p1.y, edge->p2.y,
			 t->top, t->bottom,
			 dir);
}

static void
step_edges(struct active_list *active, int count)
{
	struct edge *edge;

	count *= FAST_SAMPLES_Y;
	for (edge = active->head.next; edge != &active->tail; edge = edge->next) {
		edge->height_left -= count;
		if (! edge->height_left) {
			edge->prev->next = edge->next;
			edge->next->prev = edge->prev;
		}
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
		     coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static void
tor_blt_span_mono_unbounded_clipped(struct sna *sna,
				    struct sna_composite_spans_op *op,
				    pixman_region16_t *clip,
				    const BoxRec *box,
				    int coverage)
{
	tor_blt_span_clipped(sna, op, clip, box,
			     coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
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
	struct cell *cell = cells->head.next;
	BoxRec box;
	int cover = 0;

	/* Skip cells to the left of the clip region. */
	while (cell->x < xmin) {
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

		if (x >= xmax)
			break;

		DBG(("%s: cell=(%d, %d, %d), cover=%d, max=%d\n", __FUNCTION__,
		     cell->x, cell->covered_height, cell->uncovered_area,
		     cover, xmax));

		box.x2 = x;
		if (box.x2 > box.x1 && (unbounded || cover)) {
			DBG(("%s: span (%d, %d)x(%d, %d) @ %d\n", __FUNCTION__,
			     box.x1, box.y1,
			     box.x2 - box.x1,
			     box.y2 - box.y1,
			     cover));
			span(sna, op, clip, &box, cover);
		}
		box.x1 = box.x2;

		cover += cell->covered_height*FAST_SAMPLES_X*2;

		if (cell->uncovered_area) {
			int area = cover - cell->uncovered_area;
			box.x2 = x + 1;
			if (unbounded || area) {
				DBG(("%s: span (%d, %d)x(%d, %d) @ %d\n", __FUNCTION__,
				     box.x1, box.y1,
				     box.x2 - box.x1,
				     box.y2 - box.y1,
				     area));
				span(sna, op, clip, &box, area);
			}
			box.x1 = box.x2;
		}
	}

	box.x2 = xmax;
	if (box.x2 > box.x1 && (unbounded || cover)) {
		DBG(("%s: span (%d, %d)x(%d, %d) @ %d\n", __FUNCTION__,
		     box.x1, box.y1,
		     box.x2 - box.x1,
		     box.y2 - box.y1,
		     cover));
		span(sna, op, clip, &box, cover);
	}
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
	struct edge *buckets[FAST_SAMPLES_Y] = { 0 };

	DBG(("%s: unbounded=%d\n", __FUNCTION__, unbounded));

	/* Render each pixel row. */
	for (i = 0; i < h; i = j) {
		int do_full_step = 0;

		j = i + 1;

		/* Determine if we can ignore this row or use the full pixel
		 * stepper. */
		if (!polygon->y_buckets[i]) {
			if (active->head.next == &active->tail) {
				active->min_height = INT_MAX;
				active->is_vertical = 1;
				for (; j < h && !polygon->y_buckets[j]; j++)
					;
				DBG(("%s: no new edges and no exisiting edges, skipping, %d -> %d\n",
				     __FUNCTION__, i, j));

				if (unbounded)
					tor_blt_empty(sna, op, clip, span, i+ymin, j-i, xmin, xmax);
				continue;
			}

			do_full_step = can_full_step(active);
		}

		DBG(("%s: y=%d [%d], do_full_step=%d, new edges=%d, min_height=%d, vertical=%d\n",
		     __FUNCTION__,
		     i, i+ymin, do_full_step,
		     polygon->y_buckets[i] != NULL,
		     active->min_height,
		     active->is_vertical));
		if (do_full_step) {
			nonzero_row(active, coverages);

			if (active->is_vertical) {
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
			grid_scaled_y_t suby;

			fill_buckets(active, polygon->y_buckets[i], buckets);

			/* Subsample this row. */
			for (suby = 0; suby < FAST_SAMPLES_Y; suby++) {
				if (buckets[suby]) {
					merge_edges(active, buckets[suby]);
					buckets[suby] = NULL;
				}

				nonzero_subrow(active, coverages);
			}
		}

		if (coverages->head.next != &coverages->tail) {
			tor_blt(sna, op, clip, span, coverages,
				i+ymin, j-i, xmin, xmax,
				unbounded);
			cell_list_reset(coverages);
		} else if (unbounded)
			tor_blt_empty(sna, op, clip, span, i+ymin, j-i, xmin, xmax);

		active->min_height -= FAST_SAMPLES_Y;
	}
}

struct mono_edge {
	struct mono_edge *next, *prev;

	int32_t height_left;
	int32_t dir;
	int32_t vertical;

	int32_t dy;
	struct quorem x;
	struct quorem dxdy;
};

struct mono_polygon {
	int num_edges;
	struct mono_edge *edges;
	struct mono_edge **y_buckets;

	struct mono_edge *y_buckets_embedded[64];
	struct mono_edge edges_embedded[32];
};

struct mono {
	/* Leftmost edge on the current scan line. */
	struct mono_edge head, tail;
	int is_vertical;

	struct sna *sna;
	struct sna_composite_op op;
	pixman_region16_t clip;

	struct mono_polygon polygon;
};

#define I(x) pixman_fixed_to_int ((x) + pixman_fixed_1_minus_e/2)

static bool
mono_polygon_init(struct mono_polygon *polygon, BoxPtr box, int num_edges)
{
	unsigned h = box->y2 - box->y1;

	polygon->y_buckets = polygon->y_buckets_embedded;
	if (h > ARRAY_SIZE (polygon->y_buckets_embedded)) {
		polygon->y_buckets = malloc (h * sizeof (struct mono_edge *));
		if (unlikely (NULL == polygon->y_buckets))
			return false;
	}

	polygon->num_edges = 0;
	polygon->edges = polygon->edges_embedded;
	if (num_edges > (int)ARRAY_SIZE (polygon->edges_embedded)) {
		polygon->edges = malloc (num_edges * sizeof (struct mono_edge));
		if (unlikely (polygon->edges == NULL)) {
			if (polygon->y_buckets != polygon->y_buckets_embedded)
				free(polygon->y_buckets);
			return false;
		}
	}

	memset(polygon->y_buckets, 0, h * sizeof (struct edge *));
	return true;
}

static void
mono_polygon_fini(struct mono_polygon *polygon)
{
	if (polygon->y_buckets != polygon->y_buckets_embedded)
		free(polygon->y_buckets);

	if (polygon->edges != polygon->edges_embedded)
		free(polygon->edges);
}

static void
mono_add_line(struct mono *mono,
	      int dst_x, int dst_y,
	      xFixed top, xFixed bottom,
	      xPointFixed *p1, xPointFixed *p2,
	      int dir)
{
	struct mono_polygon *polygon = &mono->polygon;
	struct mono_edge *e;
	pixman_fixed_t dx;
	pixman_fixed_t dy;
	int y, ytop, ybot;

	DBG(("%s: top=%d, bottom=%d, line=(%d, %d), (%d, %d) delta=%dx%d, dir=%d\n",
	     __FUNCTION__,
	     (int)top, (int)bottom,
	     (int)p1->x, (int)p1->y, (int)p2->x, (int)p2->y,
	     dst_x, dst_y,
	     dir));

	if (top > bottom) {
		xPointFixed *t;

		y = top;
		top = bottom;
		bottom = y;

		t = p1;
		p1 = p2;
		p2 = t;

		dir = -dir;
	}

	y = I(top) + dst_y;
	ytop = MAX(y, mono->clip.extents.y1);

	y = I(bottom) + dst_y;
	ybot = MIN(y, mono->clip.extents.y2);

	if (ybot <= ytop) {
		DBG(("discard clipped line\n"));
		return;
	}

	e = polygon->edges + polygon->num_edges++;
	e->height_left = ybot - ytop;
	e->dir = dir;

	dx = p2->x - p1->x;
	dy = p2->y - p1->y;

	if (dx == 0) {
		e->vertical = TRUE;
		e->x.quo = p1->x;
		e->x.rem = 0;
		e->dxdy.quo = 0;
		e->dxdy.rem = 0;
		e->dy = 0;
	} else {
		e->vertical = FALSE;
		e->dxdy = floored_muldivrem (dx, pixman_fixed_1, dy);
		e->dy = dy;

		e->x = floored_muldivrem ((ytop-dst_y) * pixman_fixed_1 + pixman_fixed_1_minus_e/2 - p1->y,
					  dx, dy);
		e->x.quo += p1->x;
		e->x.rem -= dy;
	}
	e->x.quo += dst_x*pixman_fixed_1;

	{
		struct mono_edge **ptail = &polygon->y_buckets[ytop - mono->clip.extents.y1];
		if (*ptail)
			(*ptail)->prev = e;
		e->next = *ptail;
		e->prev = NULL;
		*ptail = e;
	}
}

static struct mono_edge *
mono_merge_sorted_edges(struct mono_edge *head_a, struct mono_edge *head_b)
{
	struct mono_edge *head, **next, *prev;
	int32_t x;

	prev = head_a->prev;
	next = &head;
	if (head_a->x.quo <= head_b->x.quo) {
		head = head_a;
	} else {
		head = head_b;
		head_b->prev = prev;
		goto start_with_b;
	}

	do {
		x = head_b->x.quo;
		while (head_a != NULL && head_a->x.quo <= x) {
			prev = head_a;
			next = &head_a->next;
			head_a = head_a->next;
		}

		head_b->prev = prev;
		*next = head_b;
		if (head_a == NULL)
			return head;

start_with_b:
		x = head_a->x.quo;
		while (head_b != NULL && head_b->x.quo <= x) {
			prev = head_b;
			next = &head_b->next;
			head_b = head_b->next;
		}

		head_a->prev = prev;
		*next = head_a;
		if (head_b == NULL)
			return head;
	} while (1);
}

static struct mono_edge *
mono_sort_edges(struct mono_edge *list,
		unsigned int level,
		struct mono_edge **head_out)
{
	struct mono_edge *head_other, *remaining;
	unsigned int i;

	head_other = list->next;

	if (head_other == NULL) {
		*head_out = list;
		return NULL;
	}

	remaining = head_other->next;
	if (list->x.quo <= head_other->x.quo) {
		*head_out = list;
		head_other->next = NULL;
	} else {
		*head_out = head_other;
		head_other->prev = list->prev;
		head_other->next = list;
		list->prev = head_other;
		list->next = NULL;
	}

	for (i = 0; i < level && remaining; i++) {
		remaining = mono_sort_edges(remaining, i, &head_other);
		*head_out = mono_merge_sorted_edges(*head_out, head_other);
	}

	return remaining;
}

static struct mono_edge *
mono_merge_unsorted_edges(struct mono_edge *head, struct mono_edge *unsorted)
{
	mono_sort_edges(unsorted, UINT_MAX, &unsorted);
	return mono_merge_sorted_edges(head, unsorted);
}

#if DEBUG_TRAPEZOIDS
static inline void
__dbg_mono_edges(const char *function, struct mono_edge *edges)
{
	ErrorF("%s: ", function);
	while (edges) {
		if (edges->x.quo < INT16_MAX << 16) {
			ErrorF("(%d.%06d)+(%d.%06d)x%d, ",
			       edges->x.quo, edges->x.rem,
			       edges->dxdy.quo, edges->dxdy.rem,
			       edges->dy*edges->dir);
		}
		edges = edges->next;
	}
	ErrorF("\n");
}
#define DBG_MONO_EDGES(x) __dbg_mono_edges(__FUNCTION__, x)
static inline void
VALIDATE_MONO_EDGES(struct mono_edge *edges)
{
	int prev_x = edges->x.quo;
	while ((edges = edges->next)) {
		assert(edges->x.quo >= prev_x);
		prev_x = edges->x.quo;
	}
}

#else
#define DBG_MONO_EDGES(x)
#define VALIDATE_MONO_EDGES(x)
#endif

inline static void
mono_merge_edges(struct mono *c, struct mono_edge *edges)
{
	struct mono_edge *e;

	DBG_MONO_EDGES(edges);

	for (e = edges; c->is_vertical && e; e = e->next)
		c->is_vertical = e->vertical;

	c->head.next = mono_merge_unsorted_edges(c->head.next, edges);
}

inline static void
mono_span(struct mono *c, int x1, int x2, BoxPtr box)
{
	if (x1 < c->clip.extents.x1)
		x1 = c->clip.extents.x1;
	if (x2 > c->clip.extents.x2)
		x2 = c->clip.extents.x2;
	if (x2 <= x1)
		return;

	DBG(("%s [%d, %d]\n", __FUNCTION__, x1, x2));

	box->x1 = x1;
	box->x2 = x2;

	if (c->clip.data) {
		pixman_region16_t region;

		pixman_region_init_rects(&region, box, 1);
		RegionIntersect(&region, &region, &c->clip);
		if (REGION_NUM_RECTS(&region)) {
			c->op.boxes(c->sna, &c->op,
				    REGION_RECTS(&region),
				    REGION_NUM_RECTS(&region));
			apply_damage(&c->op, &region);
		}
		pixman_region_fini(&region);
	} else {
		c->op.box(c->sna, &c->op, box);
		apply_damage_box(&c->op, box);
	}
}

inline static void
mono_row(struct mono *c, int16_t y, int16_t h)
{
	struct mono_edge *edge = c->head.next;
	int prev_x = INT_MIN;
	int16_t xstart = INT16_MIN;
	int winding = 0;
	BoxRec box;

	DBG_MONO_EDGES(edge);
	VALIDATE_MONO_EDGES(&c->head);

	box.y1 = c->clip.extents.y1 + y;
	box.y2 = box.y1 + h;

	while (&c->tail != edge) {
		struct mono_edge *next = edge->next;
		int16_t xend = I(edge->x.quo);

		if (--edge->height_left) {
			edge->x.quo += edge->dxdy.quo;
			edge->x.rem += edge->dxdy.rem;
			if (edge->x.rem >= 0) {
				++edge->x.quo;
				edge->x.rem -= edge->dy;
			}

			if (edge->x.quo < prev_x) {
				struct mono_edge *pos = edge->prev;
				pos->next = next;
				next->prev = pos;
				do {
					pos = pos->prev;
				} while (edge->x.quo < pos->x.quo);
				pos->next->prev = edge;
				edge->next = pos->next;
				edge->prev = pos;
				pos->next = edge;
			} else
				prev_x = edge->x.quo;
		} else {
			edge->prev->next = next;
			next->prev = edge->prev;
		}

		winding += edge->dir;
		if (winding == 0) {
			assert(I(next->x.quo) >= xend);
			if (I(next->x.quo) > xend + 1) {
				mono_span(c, xstart, xend, &box);
				xstart = INT16_MIN;
			}
		} else if (xstart == INT16_MIN)
			xstart = xend;

		edge = next;
	}

	DBG_MONO_EDGES(c->head.next);
	VALIDATE_MONO_EDGES(&c->head);
}

static bool
mono_init(struct mono *c, int num_edges)
{
	if (!mono_polygon_init(&c->polygon, &c->clip.extents, num_edges))
		return false;

	c->head.vertical = 1;
	c->head.height_left = INT_MAX;
	c->head.x.quo = INT16_MIN << 16;
	c->head.prev = NULL;
	c->head.next = &c->tail;
	c->tail.prev = &c->head;
	c->tail.next = NULL;
	c->tail.x.quo = INT16_MAX << 16;
	c->tail.height_left = INT_MAX;
	c->tail.vertical = 1;

	c->is_vertical = 1;

	return true;
}

static void
mono_fini(struct mono *mono)
{
	mono_polygon_fini(&mono->polygon);
}

static void
mono_step_edges(struct mono *c, int count)
{
	struct mono_edge *edge;

	for (edge = c->head.next; edge != &c->tail; edge = edge->next) {
		edge->height_left -= count;
		if (! edge->height_left) {
			edge->prev->next = edge->next;
			edge->next->prev = edge->prev;
		}
	}
}

static void
mono_render(struct mono *mono)
{
	struct mono_polygon *polygon = &mono->polygon;
	int i, j, h = mono->clip.extents.y2 - mono->clip.extents.y1;

	for (i = 0; i < h; i = j) {
		j = i + 1;

		if (polygon->y_buckets[i])
			mono_merge_edges(mono, polygon->y_buckets[i]);

		if (mono->is_vertical) {
			struct mono_edge *e = mono->head.next;
			int min_height = h - i;

			while (e != &mono->tail) {
				if (e->height_left < min_height)
					min_height = e->height_left;
				e = e->next;
			}

			while (--min_height >= 1 && polygon->y_buckets[j] == NULL)
				j++;
			if (j != i + 1)
				mono_step_edges(mono, j - (i + 1));
		}

		mono_row(mono, i, j-i);

		/* XXX recompute after dropping edges? */
		if (mono->head.next == &mono->tail)
			mono->is_vertical = 1;
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
composite_aligned_boxes(struct sna *sna,
			CARD8 op,
			PicturePtr src,
			PicturePtr dst,
			PictFormatPtr maskFormat,
			INT16 src_x, INT16 src_y,
			int ntrap, xTrapezoid *traps)
{
	BoxRec stack_boxes[64], *boxes, extents;
	pixman_region16_t region, clip;
	struct sna_composite_op tmp;
	Bool ret = true;
	int dx, dy, n, num_boxes;

	if (NO_ALIGNED_BOXES)
		return false;

	DBG(("%s\n", __FUNCTION__));

	boxes = stack_boxes;
	if (ntrap > (int)ARRAY_SIZE(stack_boxes))
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
		goto free_boxes;

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
free_boxes:
	if (boxes != stack_boxes)
		free(boxes);

	return ret;
}

static inline int grid_coverage(int samples, pixman_fixed_t f)
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
		opacity *= grid_coverage(SAMPLES_X, trap->right.p1.x) - grid_coverage(SAMPLES_X, trap->left.p1.x);

		if (opacity)
			composite_unaligned_box(sna, tmp, &box,
						opacity/255., clip);
	} else {
		if (pixman_fixed_frac(trap->left.p1.x)) {
			box.x1 = x1;
			box.x2 = x1++;

			opacity = covered;
			opacity *= SAMPLES_X - grid_coverage(SAMPLES_X, trap->left.p1.x);

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
			opacity *= grid_coverage(SAMPLES_X, trap->right.p1.x);

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
					     grid_coverage(SAMPLES_Y, trap->bottom) - grid_coverage(SAMPLES_Y, trap->top),
					     clip);
	} else {
		if (pixman_fixed_frac(trap->top)) {
			composite_unaligned_trap_row(sna, tmp, trap, dx,
						     y1, y1 + 1,
						     SAMPLES_Y - grid_coverage(SAMPLES_Y, trap->top),
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
						     grid_coverage(SAMPLES_Y, trap->bottom),
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
			    covered * (grid_coverage(SAMPLES_X, trap->right.p1.x) - grid_coverage(SAMPLES_X, trap->left.p1.x)));
	} else {
		if (pixman_fixed_frac(trap->left.p1.x))
			blt_opacity(scratch,
				    x1, x1+1,
				    y1, y2,
				    covered * (SAMPLES_X - grid_coverage(SAMPLES_X, trap->left.p1.x)));

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
				    covered * grid_coverage(SAMPLES_X, trap->right.p1.x));
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
					      grid_coverage(SAMPLES_Y, t->bottom) - grid_coverage(SAMPLES_Y, t->top));
		} else {
			if (pixman_fixed_frac(t->top))
				blt_unaligned_box_row(scratch, &extents, t, y1, y1 + 1,
						      SAMPLES_Y - grid_coverage(SAMPLES_Y, t->top));

			if (y2 > y1 + 1)
				blt_unaligned_box_row(scratch, &extents, t, y1+1, y2,
						      SAMPLES_Y);

			if (pixman_fixed_frac(t->bottom))
				blt_unaligned_box_row(scratch, &extents, t, y2, y2+1,
						      grid_coverage(SAMPLES_Y, t->bottom));
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
composite_unaligned_boxes(struct sna *sna,
			  CARD8 op,
			  PicturePtr src,
			  PicturePtr dst,
			  PictFormatPtr maskFormat,
			  INT16 src_x, INT16 src_y,
			  int ntrap, xTrapezoid *traps)
{
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
					 COMPOSITE_SPANS_RECTILINEAR,
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

static inline int pixman_fixed_to_grid (pixman_fixed_t v)
{
	return (v + FAST_SAMPLES_mask/2) >> (16 - FAST_SAMPLES_shift);
}

static inline bool
project_trapezoid_onto_grid(const xTrapezoid *in,
			    int dx, int dy,
			    xTrapezoid *out)
{
	out->left.p1.x = dx + pixman_fixed_to_grid(in->left.p1.x);
	out->left.p1.y = dy + pixman_fixed_to_grid(in->left.p1.y);
	out->left.p2.x = dx + pixman_fixed_to_grid(in->left.p2.x);
	out->left.p2.y = dy + pixman_fixed_to_grid(in->left.p2.y);

	out->right.p1.x = dx + pixman_fixed_to_grid(in->right.p1.x);
	out->right.p1.y = dy + pixman_fixed_to_grid(in->right.p1.y);
	out->right.p2.x = dx + pixman_fixed_to_grid(in->right.p2.x);
	out->right.p2.y = dy + pixman_fixed_to_grid(in->right.p2.y);

	out->top = dy + pixman_fixed_to_grid(in->top);
	out->bottom = dy + pixman_fixed_to_grid(in->bottom);

	return xTrapezoidValid(out);
}

static bool
is_mono(PicturePtr dst, PictFormatPtr mask)
{
	return mask ? mask->depth < 8 : dst->polyEdge==PolyEdgeSharp;
}

static span_func_t
choose_span(PicturePtr dst,
	    PictFormatPtr maskFormat,
	    uint8_t op,
	    RegionPtr clip)
{
	span_func_t span;

	if (is_mono(dst, maskFormat)) {
		/* XXX An imprecise approximation */
		if (maskFormat && !operator_is_bounded(op)) {
			span = tor_blt_span_mono_unbounded;
			if (REGION_NUM_RECTS(clip) > 1)
				span = tor_blt_span_mono_unbounded_clipped;
		} else {
			span = tor_blt_span_mono;
			if (REGION_NUM_RECTS(clip) > 1)
				span = tor_blt_span_mono_clipped;
		}
	} else {
		span = tor_blt_span;
		if (REGION_NUM_RECTS(clip) > 1)
			span = tor_blt_span_clipped;
	}

	return span;
}

static bool
mono_trapezoids_span_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			       INT16 src_x, INT16 src_y,
			       int ntrap, xTrapezoid *traps)
{
	struct mono mono;
	BoxRec extents;
	int16_t dst_x, dst_y;
	int16_t dx, dy;
	int n;

	if (NO_SCAN_CONVERTER)
		return false;

	dst_x = pixman_fixed_to_int(traps[0].left.p1.x);
	dst_y = pixman_fixed_to_int(traps[0].left.p1.y);

	miTrapezoidBounds(ntrap, traps, &extents);
	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (!sna_compute_composite_region(&mono.clip,
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

	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d) src -> (%d, %d)\n",
	     __FUNCTION__,
	     mono.clip.extents.x1, mono.clip.extents.y1,
	     mono.clip.extents.x2, mono.clip.extents.y2,
	     dx, dy,
	     src_x + mono.clip.extents.x1 - dst_x - dx,
	     src_y + mono.clip.extents.y1 - dst_y - dy));

	mono.sna = to_sna_from_drawable(dst->pDrawable);
	if (!mono_init(&mono, 2*ntrap))
		return false;

	for (n = 0; n < ntrap; n++) {
		if (!xTrapezoidValid(&traps[n]))
			continue;

		if (pixman_fixed_to_int(traps[n].top) + dy >= mono.clip.extents.y2 ||
		    pixman_fixed_to_int(traps[n].bottom) + dy < mono.clip.extents.y1)
			continue;

		mono_add_line(&mono, dx, dy,
			      traps[n].top, traps[n].bottom,
			      &traps[n].left.p1, &traps[n].left.p2, 1);
		mono_add_line(&mono, dx, dy,
			      traps[n].top, traps[n].bottom,
			      &traps[n].right.p1, &traps[n].right.p2, -1);
	}

	memset(&mono.op, 0, sizeof(mono.op));
	if (!mono.sna->render.composite(mono.sna, op, src, NULL, dst,
				       src_x + mono.clip.extents.x1 - dst_x - dx,
				       src_y + mono.clip.extents.y1 - dst_y - dy,
				       0, 0,
				       mono.clip.extents.x1,  mono.clip.extents.y1,
				       mono.clip.extents.x2 - mono.clip.extents.x1,
				       mono.clip.extents.y2 - mono.clip.extents.y1,
				       &mono.op)) {
		mono_fini(&mono);
		return false;
	}
	mono_render(&mono);
	mono.op.done(mono.sna, &mono.op);
	mono_fini(&mono);

	if (!operator_is_bounded(op)) {
		xPointFixed p1, p2;

		if (!mono_init(&mono, 2+2*ntrap))
			return false;

		p1.y = mono.clip.extents.y1 * pixman_fixed_1;
		p2.y = mono.clip.extents.y2 * pixman_fixed_1;

		p1.x = mono.clip.extents.x1 * pixman_fixed_1;
		p2.x = mono.clip.extents.x1 * pixman_fixed_1;
		mono_add_line(&mono, 0, 0, p1.y, p2.y, &p1, &p2, -1);

		p1.x = mono.clip.extents.x2 * pixman_fixed_1;
		p2.x = mono.clip.extents.x2 * pixman_fixed_1;
		mono_add_line(&mono, 0, 0, p1.y, p2.y, &p1, &p2, 1);

		for (n = 0; n < ntrap; n++) {
			if (!xTrapezoidValid(&traps[n]))
				continue;

			if (pixman_fixed_to_int(traps[n].top) + dy >= mono.clip.extents.y2 ||
			    pixman_fixed_to_int(traps[n].bottom) + dy < mono.clip.extents.y1)
				continue;

			mono_add_line(&mono, dx, dy,
				      traps[n].top, traps[n].bottom,
				      &traps[n].left.p1, &traps[n].left.p2, 1);
			mono_add_line(&mono, dx, dy,
				      traps[n].top, traps[n].bottom,
				      &traps[n].right.p1, &traps[n].right.p2, -1);
		}
		memset(&mono.op, 0, sizeof(mono.op));
		if (mono.sna->render.composite(mono.sna,
					       PictOpClear,
					       src, NULL, dst,
					       0, 0,
					       0, 0,
					       mono.clip.extents.x1,  mono.clip.extents.y1,
					       mono.clip.extents.x2 - mono.clip.extents.x1,
					       mono.clip.extents.y2 - mono.clip.extents.y1,
					       &mono.op)) {
			mono_render(&mono);
			mono.op.done(mono.sna, &mono.op);
		}
		mono_fini(&mono);
	}

	REGION_UNINIT(NULL, &mono.clip);
	return true;
}

static bool
trapezoid_span_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			 PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			 int ntrap, xTrapezoid *traps)
{
	struct sna *sna;
	struct sna_composite_spans_op tmp;
	struct tor tor;
	BoxRec extents;
	pixman_region16_t clip;
	int16_t dst_x, dst_y;
	int dx, dy, n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (is_mono(dst, maskFormat))
		return mono_trapezoids_span_converter(op, src, dst,
						      src_x, src_y,
						      ntrap, traps);

	/* XXX strict adherence to the Render specification */
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

#if 0
	if (extents.y2 - extents.y1 < 64 && extents.x2 - extents.x1 < 64) {
		DBG(("%s: fallback -- traps extents too small %dx%d\n",
		     __FUNCTION__, extents.y2 - extents.y1, extents.x2 - extents.x1));
		return false;
	}
#endif

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
					 0,
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
		xTrapezoid t;

		if (!project_trapezoid_onto_grid(&traps[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(traps[n].top) + dst->pDrawable->y >= extents.y2 ||
		    pixman_fixed_to_int(traps[n].bottom) + dst->pDrawable->y < extents.y1)
			continue;

		tor_add_edge(&tor, &t, &t.left, 1);
		tor_add_edge(&tor, &t, &t.right, -1);
	}

	tor_render(sna, &tor, &tmp, &clip,
		   choose_span(dst, maskFormat, op, &clip),
		   maskFormat && !operator_is_bounded(op));

skip:
	tor_fini(&tor);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &clip);
	return true;
}

static void
tor_blt_mask(struct sna *sna,
	     struct sna_composite_spans_op *op,
	     pixman_region16_t *clip,
	     const BoxRec *box,
	     int coverage)
{
	uint8_t *ptr = (uint8_t *)op;
	int stride = (intptr_t)clip;
	int h, w;

	coverage = 256 * coverage / FAST_SAMPLES_XY;
	coverage -= coverage >> 8;

	ptr += box->y1 * stride + box->x1;

	h = box->y2 - box->y1;
	w = box->x2 - box->x1;
	if ((w | h) == 1) {
		*ptr = coverage;
	} else if (w == 1) {
		do {
			*ptr = coverage;
			ptr += stride;
		} while (--h);
	} else do {
		memset(ptr, coverage, w);
		ptr += stride;
	} while (--h);
}

static void
tor_blt_mask_mono(struct sna *sna,
		  struct sna_composite_spans_op *op,
		  pixman_region16_t *clip,
		  const BoxRec *box,
		  int coverage)
{
	tor_blt_mask(sna, op, clip, box,
		     coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static bool
trapezoid_mask_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			 PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			 int ntrap, xTrapezoid *traps)
{
	struct tor tor;
	span_func_t span;
	ScreenPtr screen = dst->pDrawable->pScreen;
	PixmapPtr scratch;
	PicturePtr mask;
	BoxRec extents;
	int16_t dst_x, dst_y;
	int dx, dy;
	int error, n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (dst->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	if (maskFormat == NULL && ntrap > 1) {
		DBG(("%s: individual rasterisation requested\n",
		     __FUNCTION__));
		do {
			/* XXX unwind errors? */
			if (!trapezoid_mask_converter(op, src, dst, NULL,
						 src_x, src_y, 1, traps++))
				return false;
		} while (--ntrap);
		return true;
	}

	miTrapezoidBounds(ntrap, traps, &extents);
	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (!sna_compute_composite_extents(&extents,
					   src, NULL, dst,
					   src_x, src_y,
					   0, 0,
					   extents.x1, extents.y1,
					   extents.x2 - extents.x1,
					   extents.y2 - extents.y1))
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	extents.y2 -= extents.y1;
	extents.x2 -= extents.x1;
	extents.x1 -= dst->pDrawable->x;
	extents.y1 -= dst->pDrawable->y;
	dst_x = extents.x1;
	dst_y = extents.y1;
	dx = -extents.x1 * FAST_SAMPLES_X;
	dy = -extents.y1 * FAST_SAMPLES_Y;
	extents.x1 = extents.y1 = 0;

	DBG(("%s: mask (%dx%d), dx=(%d, %d)\n",
	     __FUNCTION__, extents.x2, extents.y2, dx, dy));
	scratch = sna_pixmap_create_upload(screen, extents.x2, extents.y2, 8);
	if (!scratch)
		return true;

	DBG(("%s: created buffer %p, stride %d\n",
	     __FUNCTION__, scratch->devPrivate.ptr, scratch->devKind));

	if (tor_init(&tor, &extents, 2*ntrap)) {
		screen->DestroyPixmap(scratch);
		return true;
	}

	for (n = 0; n < ntrap; n++) {
		xTrapezoid t;

		if (!project_trapezoid_onto_grid(&traps[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(traps[n].top) - dst_y >= extents.y2 ||
		    pixman_fixed_to_int(traps[n].bottom) - dst_y < 0)
			continue;

		tor_add_edge(&tor, &t, &t.left, 1);
		tor_add_edge(&tor, &t, &t.right, -1);
	}

	if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
		span = tor_blt_mask_mono;
	else
		span = tor_blt_mask;

	tor_render(NULL, &tor,
		   scratch->devPrivate.ptr,
		   (void *)(intptr_t)scratch->devKind,
		   span, true);

	mask = CreatePicture(0, &scratch->drawable,
			     PictureMatchFormat(screen, 8, PICT_a8),
			     0, 0, serverClient, &error);
	screen->DestroyPixmap(scratch);
	if (mask) {
		CompositePicture(op, src, mask, dst,
				 src_x + dst_x - pixman_fixed_to_int(traps[0].left.p1.x),
				 src_y + dst_y - pixman_fixed_to_int(traps[0].left.p1.y),
				 0, 0,
				 dst_x, dst_y,
				 extents.x2, extents.y2);
		FreePicture(mask, 0);
	}
	tor_fini(&tor);

	return true;
}

struct inplace {
	uint32_t stride;
	uint8_t *ptr;
	uint8_t opacity;
};

static void
tor_blt_src(struct sna *sna,
	    struct sna_composite_spans_op *op,
	    pixman_region16_t *clip,
	    const BoxRec *box,
	    int coverage)
{
	struct inplace *in = (struct inplace *)op;
	uint8_t *ptr = in->ptr;
	int h, w;

	coverage = coverage * in->opacity / FAST_SAMPLES_XY;

	ptr += box->y1 * in->stride + box->x1;

	h = box->y2 - box->y1;
	w = box->x2 - box->x1;
	if ((w | h) == 1) {
		*ptr = coverage;
	} else if (w == 1) {
		do {
			*ptr = coverage;
			ptr += in->stride;
		} while (--h);
	} else do {
		memset(ptr, coverage, w);
		ptr += in->stride;
	} while (--h);
}

static void
tor_blt_src_clipped(struct sna *sna,
		    struct sna_composite_spans_op *op,
		    pixman_region16_t *clip,
		    const BoxRec *box,
		    int coverage)
{
	pixman_region16_t region;
	int n;

	pixman_region_init_rects(&region, box, 1);
	RegionIntersect(&region, &region, clip);
	n = REGION_NUM_RECTS(&region);
	box = REGION_RECTS(&region);
	while (n--)
		tor_blt_src(sna, op, NULL, box++, coverage);
	pixman_region_fini(&region);
}

static void
tor_blt_src_mono(struct sna *sna,
		 struct sna_composite_spans_op *op,
		 pixman_region16_t *clip,
		 const BoxRec *box,
		 int coverage)
{
	tor_blt_src(sna, op, clip, box,
		    coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static void
tor_blt_src_clipped_mono(struct sna *sna,
			 struct sna_composite_spans_op *op,
			 pixman_region16_t *clip,
			 const BoxRec *box,
			 int coverage)
{
	tor_blt_src_clipped(sna, op, clip, box,
			    coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static void
tor_blt_in(struct sna *sna,
	   struct sna_composite_spans_op *op,
	   pixman_region16_t *clip,
	   const BoxRec *box,
	   int coverage)
{
	struct inplace *in = (struct inplace *)op;
	uint8_t *ptr = in->ptr;
	int h, w, i;

	coverage = coverage * in->opacity / FAST_SAMPLES_XY;
	if (coverage == 0) {
		tor_blt_src(sna, op, clip, box, 0);
		return;
	}

	ptr += box->y1 * in->stride + box->x1;

	h = box->y2 - box->y1;
	w = box->x2 - box->x1;
	do {
		for (i = 0; i < w; i++)
			ptr[i] = (ptr[i] * coverage) >> 8;
		ptr += in->stride;
	} while (--h);
}

static void
tor_blt_in_clipped(struct sna *sna,
		   struct sna_composite_spans_op *op,
		   pixman_region16_t *clip,
		   const BoxRec *box,
		   int coverage)
{
	pixman_region16_t region;
	int n;

	pixman_region_init_rects(&region, box, 1);
	RegionIntersect(&region, &region, clip);
	n = REGION_NUM_RECTS(&region);
	box = REGION_RECTS(&region);
	while (n--)
		tor_blt_in(sna, op, NULL, box++, coverage);
	pixman_region_fini(&region);
}

static void
tor_blt_in_mono(struct sna *sna,
		struct sna_composite_spans_op *op,
		pixman_region16_t *clip,
		const BoxRec *box,
		int coverage)
{
	tor_blt_in(sna, op, clip, box,
		   coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static void
tor_blt_in_clipped_mono(struct sna *sna,
			struct sna_composite_spans_op *op,
			pixman_region16_t *clip,
			const BoxRec *box,
			int coverage)
{
	tor_blt_in_clipped(sna, op, clip, box,
			   coverage < FAST_SAMPLES_XY/2 ? 0 : FAST_SAMPLES_XY);
}

static void
tor_blt_add(struct sna *sna,
	    struct sna_composite_spans_op *op,
	    pixman_region16_t *clip,
	    const BoxRec *box,
	    int coverage)
{
	struct inplace *in = (struct inplace *)op;
	uint8_t *ptr = in->ptr;
	int h, w, v, i;

	coverage = coverage * in->opacity / FAST_SAMPLES_XY;
	if (coverage == 0)
		return;

	ptr += box->y1 * in->stride + box->x1;

	h = box->y2 - box->y1;
	w = box->x2 - box->x1;
	if ((w | h) == 1) {
		v = coverage + *ptr;
		*ptr = v >= 255 ? 255 : v;
	} else {
		do {
			for (i = 0; i < w; i++) {
				v = coverage + ptr[i];
				ptr[i] = v >= 255 ? 255 : v;
			}
			ptr += in->stride;
		} while (--h);
	}
}

static void
tor_blt_add_clipped(struct sna *sna,
		    struct sna_composite_spans_op *op,
		    pixman_region16_t *clip,
		    const BoxRec *box,
		    int coverage)
{
	pixman_region16_t region;
	int n;

	pixman_region_init_rects(&region, box, 1);
	RegionIntersect(&region, &region, clip);
	n = REGION_NUM_RECTS(&region);
	box = REGION_RECTS(&region);
	while (n--)
		tor_blt_add(sna, op, NULL, box++, coverage);
	pixman_region_fini(&region);
}

static void
tor_blt_add_mono(struct sna *sna,
		 struct sna_composite_spans_op *op,
		 pixman_region16_t *clip,
		 const BoxRec *box,
		 int coverage)
{
	if (coverage >= FAST_SAMPLES_XY/2)
		tor_blt_add(sna, op, clip, box, FAST_SAMPLES_XY);
}

static void
tor_blt_add_clipped_mono(struct sna *sna,
			 struct sna_composite_spans_op *op,
			 pixman_region16_t *clip,
			 const BoxRec *box,
			 int coverage)
{
	if (coverage >= FAST_SAMPLES_XY/2)
		tor_blt_add_clipped(sna, op, clip, box, FAST_SAMPLES_XY);
}

static bool
trapezoid_span_inplace(CARD8 op, PicturePtr src, PicturePtr dst,
		       PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
		       int ntrap, xTrapezoid *traps)
{
	struct tor tor;
	struct inplace inplace;
	span_func_t span;
	PixmapPtr pixmap;
	RegionRec region;
	uint32_t color;
	int16_t dst_x, dst_y;
	int dx, dy;
	int n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (dst->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}
	if (dst->alphaMap) {
		DBG(("%s: fallback -- dst alphamap\n",
		     __FUNCTION__));
		return false;
	}

	if (dst->format != PICT_a8 || !sna_picture_is_solid(src, &color)) {
		DBG(("%s: fallback -- can not perform operation in place, format=%x\n",
		     __FUNCTION__, dst->format));
		return false;
	}

	switch (op) {
	case PictOpSrc:
	case PictOpIn:
	case PictOpAdd:
		break;
	default:
		DBG(("%s: fallback -- can not perform op [%d] in place\n",
		     __FUNCTION__, op));
		return false;
	}

	DBG(("%s: format=%x, op=%d, color=%x\n",
	     __FUNCTION__, dst->format, op, color));

	if (maskFormat == NULL && ntrap > 1) {
		DBG(("%s: individual rasterisation requested\n",
		     __FUNCTION__));
		do {
			/* XXX unwind errors? */
			if (!trapezoid_span_inplace(op, src, dst, NULL,
						    src_x, src_y, 1, traps++))
				return false;
		} while (--ntrap);
		return true;
	}

	miTrapezoidBounds(ntrap, traps, &region.extents);
	if (region.extents.y1 >= region.extents.y2 ||
	    region.extents.x1 >= region.extents.x2)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (!sna_compute_composite_extents(&region.extents,
					   src, NULL, dst,
					   src_x, src_y,
					   0, 0,
					   region.extents.x1, region.extents.y1,
					   region.extents.x2 - region.extents.x1,
					   region.extents.y2 - region.extents.y1))
		return true;

	DBG(("%s: clipped extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (tor_init(&tor, &region.extents, 2*ntrap))
		return true;

	dx = dst->pDrawable->x * FAST_SAMPLES_X;
	dy = dst->pDrawable->y * FAST_SAMPLES_Y;

	for (n = 0; n < ntrap; n++) {
		xTrapezoid t;

		if (!project_trapezoid_onto_grid(&traps[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(traps[n].top) >= region.extents.y2 - dst->pDrawable->y ||
		    pixman_fixed_to_int(traps[n].bottom) < region.extents.y1 - dst->pDrawable->y)
			continue;

		tor_add_edge(&tor, &t, &t.left, 1);
		tor_add_edge(&tor, &t, &t.right, -1);
	}

	if (op == PictOpSrc) {
		if (dst->pCompositeClip->data) {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_src_clipped_mono;
			else
				span = tor_blt_src_clipped;
		} else {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_src_mono;
			else
				span = tor_blt_src;
		}
	} else if (op == PictOpIn) {
		if (dst->pCompositeClip->data) {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_in_clipped_mono;
			else
				span = tor_blt_in_clipped;
		} else {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_in_mono;
			else
				span = tor_blt_in;
		}
	} else {
		if (dst->pCompositeClip->data) {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_add_clipped_mono;
			else
				span = tor_blt_add_clipped;
		} else {
			if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
				span = tor_blt_add_mono;
			else
				span = tor_blt_add;
		}
	}

	region.data = NULL;
	if (!sna_drawable_move_region_to_cpu(dst->pDrawable, &region,
					     MOVE_WRITE))
		return true;

	pixmap = get_drawable_pixmap(dst->pDrawable);
	get_drawable_deltas(dst->pDrawable, pixmap, &dst_x, &dst_y);

	inplace.ptr = pixmap->devPrivate.ptr;
	inplace.ptr += dst_y * pixmap->devKind + dst_x;
	inplace.stride = pixmap->devKind;
	inplace.opacity = color >> 24;

	tor_render(NULL, &tor, (void*)&inplace,
		   dst->pCompositeClip, span, true);

	tor_fini(&tor);

	return true;
}

static bool
trapezoid_span_fallback(CARD8 op, PicturePtr src, PicturePtr dst,
			PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			int ntrap, xTrapezoid *traps)
{
	struct tor tor;
	span_func_t span;
	ScreenPtr screen = dst->pDrawable->pScreen;
	PixmapPtr scratch;
	PicturePtr mask;
	BoxRec extents;
	int16_t dst_x, dst_y;
	int dx, dy;
	int error, n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (dst->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	if (maskFormat == NULL && ntrap > 1) {
		DBG(("%s: individual rasterisation requested\n",
		     __FUNCTION__));
		do {
			/* XXX unwind errors? */
			if (!trapezoid_span_fallback(op, src, dst, NULL,
						     src_x, src_y, 1, traps++))
				return false;
		} while (--ntrap);
		return true;
	}

	miTrapezoidBounds(ntrap, traps, &extents);
	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (!sna_compute_composite_extents(&extents,
					   src, NULL, dst,
					   src_x, src_y,
					   0, 0,
					   extents.x1, extents.y1,
					   extents.x2 - extents.x1,
					   extents.y2 - extents.y1))
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	extents.y2 -= extents.y1;
	extents.x2 -= extents.x1;
	extents.x1 -= dst->pDrawable->x;
	extents.y1 -= dst->pDrawable->y;
	dst_x = extents.x1;
	dst_y = extents.y1;
	dx = -extents.x1 * FAST_SAMPLES_X;
	dy = -extents.y1 * FAST_SAMPLES_Y;
	extents.x1 = extents.y1 = 0;

	DBG(("%s: mask (%dx%d), dx=(%d, %d)\n",
	     __FUNCTION__, extents.x2, extents.y2, dx, dy));
	scratch = fbCreatePixmap(screen,
				 extents.x2, extents.y2, 8,
				 CREATE_PIXMAP_USAGE_SCRATCH);
	if (!scratch)
		return true;

	DBG(("%s: created buffer %p, stride %d\n",
	     __FUNCTION__, scratch->devPrivate.ptr, scratch->devKind));

	if (tor_init(&tor, &extents, 2*ntrap)) {
		screen->DestroyPixmap(scratch);
		return true;
	}

	for (n = 0; n < ntrap; n++) {
		xTrapezoid t;

		if (!project_trapezoid_onto_grid(&traps[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(traps[n].top) - dst_y >= extents.y2 ||
		    pixman_fixed_to_int(traps[n].bottom) - dst_y < 0)
			continue;

		tor_add_edge(&tor, &t, &t.left, 1);
		tor_add_edge(&tor, &t, &t.right, -1);
	}

	if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
		span = tor_blt_mask_mono;
	else
		span = tor_blt_mask;

	tor_render(NULL, &tor,
		   scratch->devPrivate.ptr,
		   (void *)(intptr_t)scratch->devKind,
		   span, true);

	mask = CreatePicture(0, &scratch->drawable,
			     PictureMatchFormat(screen, 8, PICT_a8),
			     0, 0, serverClient, &error);
	screen->DestroyPixmap(scratch);
	if (mask) {
		RegionRec region;

		region.extents.x1 = dst_x;
		region.extents.y1 = dst_y;
		region.extents.x2 = dst_x + extents.x2;
		region.extents.y2 = dst_y + extents.y2;
		region.data = NULL;

		if (!sna_drawable_move_region_to_cpu(dst->pDrawable, &region,
						     MOVE_READ | MOVE_WRITE))
			goto done;
		if (dst->alphaMap  &&
		    !sna_drawable_move_to_cpu(dst->alphaMap->pDrawable,
					      MOVE_READ | MOVE_WRITE))
			goto done;
		if (src->pDrawable) {
			if (!sna_drawable_move_to_cpu(src->pDrawable,
						      MOVE_READ))
				goto done;
			if (src->alphaMap &&
			    !sna_drawable_move_to_cpu(src->alphaMap->pDrawable,
						      MOVE_READ))
				goto done;
		}

		fbComposite(op, src, mask, dst,
			    src_x + dst_x - pixman_fixed_to_int(traps[0].left.p1.x),
			    src_y + dst_y - pixman_fixed_to_int(traps[0].left.p1.y),
			    0, 0,
			    dst_x, dst_y,
			    extents.x2, extents.y2);
done:
		FreePicture(mask, 0);
	}
	tor_fini(&tor);

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
	bool rectilinear, pixel_aligned;
	int n;

	DBG(("%s(op=%d, src=(%d, %d), mask=%08x, ntrap=%d)\n", __FUNCTION__,
	     op, xSrc, ySrc,
	     maskFormat ? (int)maskFormat->format : 0,
	     ntrap));

	if (ntrap == 0)
		return;

	if (NO_ACCEL)
		goto fallback;

	if (wedged(sna) || !sna->have_render) {
		DBG(("%s: fallback -- wedged=%d, have_render=%d\n",
		     __FUNCTION__, sna->kgem.wedged, sna->have_render));
		goto fallback;
	}

	if (dst->alphaMap) {
		DBG(("%s: fallback -- dst alpha map\n", __FUNCTION__));
		goto fallback;
	}

	if (too_small(dst->pDrawable) && !picture_is_gpu(src)) {
		DBG(("%s: fallback -- dst is too small, %dx%d\n",
		     __FUNCTION__,
		     dst->pDrawable->width,
		     dst->pDrawable->height));
		goto fallback;
	}

	/* scan through for fast rectangles */
	rectilinear = pixel_aligned = true;
	if (maskFormat ? maskFormat->depth == 1 : dst->polyEdge == PolyEdgeSharp) {
		for (n = 0; n < ntrap && rectilinear; n++) {
			int lx1 = pixman_fixed_to_int(traps[n].left.p1.x + pixman_fixed_1_minus_e/2);
			int lx2 = pixman_fixed_to_int(traps[n].left.p2.x + pixman_fixed_1_minus_e/2);
			int rx1 = pixman_fixed_to_int(traps[n].right.p1.x + pixman_fixed_1_minus_e/2);
			int rx2 = pixman_fixed_to_int(traps[n].right.p2.x + pixman_fixed_1_minus_e/2);
			rectilinear &= lx1 == lx2 && rx1 == rx2;
		}
	} else if (dst->polyMode != PolyModePrecise) {
		for (n = 0; n < ntrap && rectilinear; n++) {
			int lx1 = pixman_fixed_to_grid(traps[n].left.p1.x);
			int lx2 = pixman_fixed_to_grid(traps[n].left.p2.x);
			int rx1 = pixman_fixed_to_grid(traps[n].right.p1.x);
			int rx2 = pixman_fixed_to_grid(traps[n].right.p2.x);
			int top = pixman_fixed_to_grid(traps[n].top);
			int bot = pixman_fixed_to_grid(traps[n].bottom);

			rectilinear &= lx1 == lx2 && rx1 == rx2;
			pixel_aligned &= ((top | bot | lx1 | lx2 | rx1 | rx2) & FAST_SAMPLES_mask) == 0;
		}
	} else {
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
	}

	DBG(("%s: rectlinear? %d, pixel-aligned? %d\n",
	     __FUNCTION__, rectilinear, pixel_aligned));
	if (rectilinear) {
		if (pixel_aligned) {
			if (composite_aligned_boxes(sna, op, src, dst,
						    maskFormat,
						    xSrc, ySrc,
						    ntrap, traps))
			    return;
		} else {
			if (composite_unaligned_boxes(sna, op, src, dst,
						      maskFormat,
						      xSrc, ySrc,
						      ntrap, traps))
				return;
		}
	}

	if (trapezoid_span_converter(op, src, dst, maskFormat,
				     xSrc, ySrc, ntrap, traps))
		return;

	if (trapezoid_mask_converter(op, src, dst, maskFormat,
				     xSrc, ySrc, ntrap, traps))
		return;

fallback:
	if (trapezoid_span_inplace(op, src, dst, maskFormat,
				   xSrc, ySrc, ntrap, traps))
		return;

	if (trapezoid_span_fallback(op, src, dst, maskFormat,
				    xSrc, ySrc, ntrap, traps))
		return;

	DBG(("%s: fallback mask=%08x, ntrap=%d\n", __FUNCTION__,
	     maskFormat ? (unsigned)maskFormat->format : 0, ntrap));
	trapezoids_fallback(op, src, dst, maskFormat,
			    xSrc, ySrc,
			    ntrap, traps);
}

static inline bool
project_trap_onto_grid(const xTrap *in,
		       int dx, int dy,
		       xTrap *out)
{
	out->top.l = dx + pixman_fixed_to_grid(in->top.l);
	out->top.r = dx + pixman_fixed_to_grid(in->top.r);
	out->top.y = dy + pixman_fixed_to_grid(in->top.y);

	out->bot.l = dx + pixman_fixed_to_grid(in->bot.l);
	out->bot.r = dx + pixman_fixed_to_grid(in->bot.r);
	out->bot.y = dy + pixman_fixed_to_grid(in->bot.y);

	return out->bot.y > out->top.y;
}

static bool
mono_trap_span_converter(PicturePtr dst,
			 INT16 x, INT16 y,
			 int ntrap, xTrap *traps)
{
	struct mono mono;
	xRenderColor white;
	PicturePtr src;
	int error;
	int n;

	white.red = white.green = white.blue = white.alpha = 0xffff;
	src = CreateSolidPicture(0, &white, &error);
	if (src == NULL)
		return true;

	mono.clip = *dst->pCompositeClip;
	x += dst->pDrawable->x;
	y += dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d)\n",
	     __FUNCTION__,
	     mono.clip.extents.x1, mono.clip.extents.y1,
	     mono.clip.extents.x2, mono.clip.extents.y2,
	     x, y));

	mono.sna = to_sna_from_drawable(dst->pDrawable);
	if (!mono_init(&mono, 2*ntrap))
		return false;

	for (n = 0; n < ntrap; n++) {
		xPointFixed p1, p2;

		if (pixman_fixed_to_int(traps[n].top.y) + y >= mono.clip.extents.y2 ||
		    pixman_fixed_to_int(traps[n].bot.y) + y < mono.clip.extents.y1)
			continue;

		p1.y = traps[n].top.y;
		p2.y = traps[n].bot.y;

		p1.x = traps[n].top.l;
		p2.x = traps[n].bot.l;
		mono_add_line(&mono, x, y,
			      traps[n].top.y, traps[n].bot.y,
			      &p1, &p2, 1);

		p1.x = traps[n].top.r;
		p2.x = traps[n].bot.r;
		mono_add_line(&mono, x, y,
			      traps[n].top.y, traps[n].bot.y,
			      &p1, &p2, -1);
	}

	memset(&mono.op, 0, sizeof(mono.op));
	if (mono.sna->render.composite(mono.sna, PictOpAdd, src, NULL, dst,
					0, 0,
					0, 0,
					mono.clip.extents.x1,  mono.clip.extents.y1,
					mono.clip.extents.x2 - mono.clip.extents.x1,
					mono.clip.extents.y2 - mono.clip.extents.y1,
					&mono.op)) {
		mono_render(&mono);
		mono.op.done(mono.sna, &mono.op);
	}

	mono_fini(&mono);
	FreePicture(src, 0);
	return true;
}

static bool
trap_span_converter(PicturePtr dst,
		    INT16 src_x, INT16 src_y,
		    int ntrap, xTrap *trap)
{
	struct sna *sna;
	struct sna_composite_spans_op tmp;
	struct tor tor;
	BoxRec extents;
	PicturePtr src;
	xRenderColor white;
	pixman_region16_t *clip;
	int dx, dy;
	int n, error;

	if (NO_SCAN_CONVERTER)
		return false;

	if (dst->pDrawable->depth < 8)
		return false;

	if (dst->polyEdge == PolyEdgeSharp)
		return mono_trap_span_converter(dst, src_x, src_y, ntrap, trap);

	sna = to_sna_from_drawable(dst->pDrawable);
	if (!sna->render.composite_spans) {
		DBG(("%s: fallback -- composite spans not supported\n",
		     __FUNCTION__));
		return false;
	}

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	clip = dst->pCompositeClip;
	extents = *RegionExtents(clip);
	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d)\n",
	     __FUNCTION__,
	     extents.x1, extents.y1,
	     extents.x2, extents.y2,
	     dx, dy));

	white.red = white.green = white.blue = white.alpha = 0xffff;
	src = CreateSolidPicture(0, &white, &error);
	if (src == NULL)
		return true;

	memset(&tmp, 0, sizeof(tmp));
	if (!sna->render.composite_spans(sna, PictOpAdd, src, dst,
					 0, 0,
					 extents.x1,  extents.y1,
					 extents.x2 - extents.x1,
					 extents.y2 - extents.y1,
					 0,
					 &tmp)) {
		DBG(("%s: fallback -- composite spans render op not supported\n",
		     __FUNCTION__));
		FreePicture(src, 0);
		return false;
	}

	dx *= FAST_SAMPLES_X;
	dy *= FAST_SAMPLES_Y;
	if (tor_init(&tor, &extents, 2*ntrap))
		goto skip;

	for (n = 0; n < ntrap; n++) {
		xTrap t;
		xPointFixed p1, p2;

		if (!project_trap_onto_grid(&trap[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(trap[n].top.y) + dst->pDrawable->y >= extents.y2 ||
		    pixman_fixed_to_int(trap[n].bot.y) + dst->pDrawable->y < extents.y1)
			continue;

		p1.y = t.top.y;
		p2.y = t.bot.y;
		p1.x = t.top.l;
		p2.x = t.bot.l;
		polygon_add_line(tor.polygon, &p1, &p2);

		p1.y = t.bot.y;
		p2.y = t.top.y;
		p1.x = t.top.r;
		p2.x = t.bot.r;
		polygon_add_line(tor.polygon, &p1, &p2);
	}

	tor_render(sna, &tor, &tmp, clip,
		   choose_span(dst, NULL, PictOpAdd, clip), false);

skip:
	tor_fini(&tor);
	tmp.done(sna, &tmp);
	FreePicture(src, 0);
	return true;
}

#define pixman_fixed_integer_floor(V) pixman_fixed_to_int(pixman_fixed_floor(V))
#define pixman_fixed_integer_ceil(V) pixman_fixed_to_int(pixman_fixed_ceil(V))

static void mark_damaged(PixmapPtr pixmap, struct sna_pixmap *priv,
			 BoxPtr box, int16_t x, int16_t y)
{
	box->x1 += x; box->x2 += x;
	box->y1 += y; box->y2 += y;
	if (box->x1 <= 0 && box->y1 <= 0 &&
	    box->x2 >= pixmap->drawable.width &&
	    box->y2 >= pixmap->drawable.height) {
		sna_damage_destroy(&priv->cpu_damage);
		sna_damage_all(&priv->gpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);
	} else {
		sna_damage_add_box(&priv->gpu_damage, box);
		sna_damage_subtract_box(&priv->cpu_damage, box);
	}
}

static bool
trap_mask_converter(PicturePtr picture,
		    INT16 x, INT16 y,
		    int ntrap, xTrap *trap)
{
	struct sna *sna;
	struct tor tor;
	ScreenPtr screen = picture->pDrawable->pScreen;
	PixmapPtr scratch, pixmap;
	struct sna_pixmap *priv;
	BoxRec extents;
	span_func_t span;
	int dx, dy, n;

	if (NO_SCAN_CONVERTER)
		return false;

	pixmap = get_drawable_pixmap(picture->pDrawable);
	priv = sna_pixmap_move_to_gpu(pixmap, MOVE_READ | MOVE_WRITE);

	/* XXX strict adherence to the Render specification */
	if (picture->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	extents = *RegionExtents(picture->pCompositeClip);
	for (n = 0; n < ntrap; n++) {
		int v;

		v = x + pixman_fixed_integer_floor (MIN(trap[n].top.l, trap[n].bot.l));
		if (v < extents.x1)
			extents.x1 = v;

		v = x + pixman_fixed_integer_ceil (MAX(trap[n].top.r, trap[n].bot.r));
		if (v > extents.x2)
			extents.x2 = v;

		v = y + pixman_fixed_integer_floor (trap[n].top.y);
		if (v < extents.y1)
			extents.y1 = v;

		v = y + pixman_fixed_integer_ceil (trap[n].bot.y);
		if (v > extents.y2)
			extents.y2 = v;
	}

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	scratch = sna_pixmap_create_upload(screen,
					   extents.x2-extents.x1,
					   extents.y2-extents.y1,
					   8);
	if (!scratch)
		return true;

	dx = picture->pDrawable->x;
	dy = picture->pDrawable->y;
	dx *= FAST_SAMPLES_X;
	dy *= FAST_SAMPLES_Y;
	if (tor_init(&tor, &extents, 2*ntrap)) {
		screen->DestroyPixmap(scratch);
		return true;
	}

	for (n = 0; n < ntrap; n++) {
		xTrap t;
		xPointFixed p1, p2;

		if (!project_trap_onto_grid(&trap[n], dx, dy, &t))
			continue;

		if (pixman_fixed_to_int(trap[n].top.y) + picture->pDrawable->y >= extents.y2 ||
		    pixman_fixed_to_int(trap[n].bot.y) + picture->pDrawable->y < extents.y1)
			continue;

		p1.y = t.top.y;
		p2.y = t.bot.y;
		p1.x = t.top.l;
		p2.x = t.bot.l;
		polygon_add_line(tor.polygon, &p1, &p2);

		p1.y = t.bot.y;
		p2.y = t.top.y;
		p1.x = t.top.r;
		p2.x = t.bot.r;
		polygon_add_line(tor.polygon, &p1, &p2);
	}

	if (picture->polyEdge == PolyEdgeSharp)
		span = tor_blt_mask_mono;
	else
		span = tor_blt_mask;

	tor_render(NULL, &tor,
		   scratch->devPrivate.ptr,
		   (void *)(intptr_t)scratch->devKind,
		   span, true);

	tor_fini(&tor);

	/* XXX clip boxes */
	get_drawable_deltas(picture->pDrawable, pixmap, &x, &y);
	sna = to_sna_from_screen(screen);
	sna->render.copy_boxes(sna, GXcopy,
			       scratch, sna_pixmap_get_bo(scratch), -extents.x1, -extents.x1,
			       pixmap, priv->gpu_bo, x, y,
			       &extents, 1);
	mark_damaged(pixmap, priv, &extents ,x, y);

	screen->DestroyPixmap(scratch);
	return true;
}

static bool
trap_upload(PicturePtr picture,
	    INT16 x, INT16 y,
	    int ntrap, xTrap *trap)
{
	ScreenPtr screen = picture->pDrawable->pScreen;
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap = get_drawable_pixmap(picture->pDrawable);
	PixmapPtr scratch;
	struct sna_pixmap *priv;
	BoxRec extents;
	pixman_image_t *image;
	int width, height, depth;
	int n;

	priv = sna_pixmap_move_to_gpu(pixmap, MOVE_READ | MOVE_WRITE);
	if (priv == NULL)
		return false;

	extents = *RegionExtents(picture->pCompositeClip);
	for (n = 0; n < ntrap; n++) {
		int v;

		v = x + pixman_fixed_integer_floor (MIN(trap[n].top.l, trap[n].bot.l));
		if (v < extents.x1)
			extents.x1 = v;

		v = x + pixman_fixed_integer_ceil (MAX(trap[n].top.r, trap[n].bot.r));
		if (v > extents.x2)
			extents.x2 = v;

		v = y + pixman_fixed_integer_floor (trap[n].top.y);
		if (v < extents.y1)
			extents.y1 = v;

		v = y + pixman_fixed_integer_ceil (trap[n].bot.y);
		if (v > extents.y2)
			extents.y2 = v;
	}

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	width  = extents.x2 - extents.x1;
	height = extents.y2 - extents.y1;
	depth = picture->pDrawable->depth;

	DBG(("%s: tmp (%dx%d) depth=%d\n",
	     __FUNCTION__, width, height, depth));
	scratch = sna_pixmap_create_upload(screen, width, height, depth);
	if (!scratch)
		return true;

	memset(scratch->devPrivate.ptr, 0, scratch->devKind*height);
	image = pixman_image_create_bits(picture->format, width, height,
					 scratch->devPrivate.ptr,
					 scratch->devKind);
	if (image) {
		pixman_add_traps (image, -extents.x1, -extents.y1,
				  ntrap, (pixman_trap_t *)trap);

		pixman_image_unref(image);
	}

	/* XXX clip boxes */
	get_drawable_deltas(picture->pDrawable, pixmap, &x, &y);
	sna->render.copy_boxes(sna, GXcopy,
			       scratch, sna_pixmap_get_bo(scratch), -extents.x1, -extents.x1,
			       pixmap, priv->gpu_bo, x, y,
			       &extents, 1);
	mark_damaged(pixmap, priv, &extents, x, y);

	screen->DestroyPixmap(scratch);
	return true;
}

void
sna_add_traps(PicturePtr picture, INT16 x, INT16 y, int n, xTrap *t)
{
	DBG(("%s (%d, %d) x %d\n", __FUNCTION__, x, y, n));

	if (is_gpu(picture->pDrawable)) {
		if (trap_span_converter(picture, x, y, n, t))
			return;

		if (trap_mask_converter(picture, x, y, n, t))
			return;

		if (trap_upload(picture, x, y, n, t))
			return;
	}

	DBG(("%s -- fallback\n", __FUNCTION__));
	if (sna_drawable_move_to_cpu(picture->pDrawable,
				     MOVE_READ | MOVE_WRITE))
		fbAddTraps(picture, x, y, n, t);
}

static inline void
project_point_onto_grid(const xPointFixed *in,
			int dx, int dy,
			xPointFixed *out)
{
	out->x = dx + pixman_fixed_to_grid(in->x);
	out->y = dy + pixman_fixed_to_grid(in->y);
}

static inline bool
xTriangleValid(const xTriangle *t)
{
	xPointFixed v1, v2;

	v1.x = t->p2.x - t->p1.x;
	v1.y = t->p2.y - t->p1.y;

	v2.x = t->p3.x - t->p1.x;
	v2.y = t->p3.y - t->p1.y;

	/* if the length of any edge is zero, the area must be zero */
	if (v1.x == 0 && v1.y == 0)
		return FALSE;
	if (v2.x == 0 && v2.y == 0)
		return FALSE;

	/* if the cross-product is zero, so it the size */
	return v2.y * v1.x != v1.y * v2.x;
}

static inline bool
project_triangle_onto_grid(const xTriangle *in,
			   int dx, int dy,
			   xTriangle *out)
{
	project_point_onto_grid(&in->p1, dx, dy, &out->p1);
	project_point_onto_grid(&in->p2, dx, dy, &out->p2);
	project_point_onto_grid(&in->p3, dx, dy, &out->p3);

	return xTriangleValid(out);
}

static bool
mono_triangles_span_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			      INT16 src_x, INT16 src_y,
			      int count, xTriangle *tri)
{
	struct mono mono;
	BoxRec extents;
	int16_t dst_x, dst_y;
	int16_t dx, dy;
	int n;

	mono.sna = to_sna_from_drawable(dst->pDrawable);

	dst_x = pixman_fixed_to_int(tri[0].p1.x);
	dst_y = pixman_fixed_to_int(tri[0].p1.y);

	miTriangleBounds(count, tri, &extents);
	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	if (!sna_compute_composite_region(&mono.clip,
					  src, NULL, dst,
					  src_x + extents.x1 - dst_x,
					  src_y + extents.y1 - dst_y,
					  0, 0,
					  extents.x1, extents.y1,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: triangles do not intersect drawable clips\n",
		     __FUNCTION__)) ;
		return true;
	}

	dx = dst->pDrawable->x;
	dy = dst->pDrawable->y;

	DBG(("%s: after clip -- extents (%d, %d), (%d, %d), delta=(%d, %d) src -> (%d, %d)\n",
	     __FUNCTION__,
	     mono.clip.extents.x1, mono.clip.extents.y1,
	     mono.clip.extents.x2, mono.clip.extents.y2,
	     dx, dy,
	     src_x + mono.clip.extents.x1 - dst_x - dx,
	     src_y + mono.clip.extents.y1 - dst_y - dy));

	if (mono_init(&mono, 3*count))
		return false;

	for (n = 0; n < count; n++) {
		mono_add_line(&mono, dx, dy,
			      tri[n].p1.y, tri[n].p2.y,
			      &tri[n].p1, &tri[n].p2, 1);
		mono_add_line(&mono, dx, dy,
			      tri[n].p2.y, tri[n].p3.y,
			      &tri[n].p2, &tri[n].p3, 1);
		mono_add_line(&mono, dx, dy,
			      tri[n].p3.y, tri[n].p1.y,
			      &tri[n].p3, &tri[n].p1, 1);
	}

	memset(&mono.op, 0, sizeof(mono.op));
	if (mono.sna->render.composite(mono.sna, op, src, NULL, dst,
				       src_x + mono.clip.extents.x1 - dst_x - dx,
				       src_y + mono.clip.extents.y1 - dst_y - dy,
				       0, 0,
				       mono.clip.extents.x1,  mono.clip.extents.y1,
				       mono.clip.extents.x2 - mono.clip.extents.x1,
				       mono.clip.extents.y2 - mono.clip.extents.y1,
				       &mono.op)) {
		mono_render(&mono);
		mono.op.done(mono.sna, &mono.op);
	}

	if (!operator_is_bounded(op)) {
		xPointFixed p1, p2;

		if (!mono_init(&mono, 2+3*count))
			return false;

		p1.y = mono.clip.extents.y1 * pixman_fixed_1;
		p2.y = mono.clip.extents.y2 * pixman_fixed_1;

		p1.x = mono.clip.extents.x1 * pixman_fixed_1;
		p2.x = mono.clip.extents.x1 * pixman_fixed_1;
		mono_add_line(&mono, 0, 0, p1.y, p2.y, &p1, &p2, -1);

		p1.x = mono.clip.extents.x2 * pixman_fixed_1;
		p2.x = mono.clip.extents.x2 * pixman_fixed_1;
		mono_add_line(&mono, 0, 0, p1.y, p2.y, &p1, &p2, 1);

		for (n = 0; n < count; n++) {
			mono_add_line(&mono, dx, dy,
				      tri[n].p1.y, tri[n].p2.y,
				      &tri[n].p1, &tri[n].p2, 1);
			mono_add_line(&mono, dx, dy,
				      tri[n].p2.y, tri[n].p3.y,
				      &tri[n].p2, &tri[n].p3, 1);
			mono_add_line(&mono, dx, dy,
				      tri[n].p3.y, tri[n].p1.y,
				      &tri[n].p3, &tri[n].p1, 1);
		}

		memset(&mono.op, 0, sizeof(mono.op));
		if (mono.sna->render.composite(mono.sna,
					       PictOpClear,
					       src, NULL, dst,
					       0, 0,
					       0, 0,
					       mono.clip.extents.x1,  mono.clip.extents.y1,
					       mono.clip.extents.x2 - mono.clip.extents.x1,
					       mono.clip.extents.y2 - mono.clip.extents.y1,
					       &mono.op)) {
			mono_render(&mono);
			mono.op.done(mono.sna, &mono.op);
		}
		mono_fini(&mono);
	}

	mono_fini(&mono);
	REGION_UNINIT(NULL, &mono.clip);
	return true;
}

static bool
triangles_span_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			 PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			 int count, xTriangle *tri)
{
	struct sna *sna;
	struct sna_composite_spans_op tmp;
	struct tor tor;
	BoxRec extents;
	pixman_region16_t clip;
	int16_t dst_x, dst_y;
	int dx, dy, n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (is_mono(dst, maskFormat))
		return mono_triangles_span_converter(op, src, dst,
						     src_x, src_y,
						     count, tri);

	/* XXX strict adherence to the Render specification */
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

	dst_x = pixman_fixed_to_int(tri[0].p1.x);
	dst_y = pixman_fixed_to_int(tri[0].p1.y);

	miTriangleBounds(count, tri, &extents);
	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

#if 0
	if (extents.y2 - extents.y1 < 64 && extents.x2 - extents.x1 < 64) {
		DBG(("%s: fallback -- traps extents too small %dx%d\n",
		     __FUNCTION__, extents.y2 - extents.y1, extents.x2 - extents.x1));
		return false;
	}
#endif

	if (!sna_compute_composite_region(&clip,
					  src, NULL, dst,
					  src_x + extents.x1 - dst_x,
					  src_y + extents.y1 - dst_y,
					  0, 0,
					  extents.x1, extents.y1,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: triangles do not intersect drawable clips\n",
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
					 0,
					 &tmp)) {
		DBG(("%s: fallback -- composite spans render op not supported\n",
		     __FUNCTION__));
		return false;
	}

	dx *= FAST_SAMPLES_X;
	dy *= FAST_SAMPLES_Y;
	if (tor_init(&tor, &extents, 3*count))
		goto skip;

	for (n = 0; n < count; n++) {
		xTriangle t;

		if (!project_triangle_onto_grid(&tri[n], dx, dy, &t))
			continue;

		polygon_add_line(tor.polygon, &t.p1, &t.p2);
		polygon_add_line(tor.polygon, &t.p2, &t.p3);
		polygon_add_line(tor.polygon, &t.p3, &t.p1);
	}

	tor_render(sna, &tor, &tmp, &clip,
		   choose_span(dst, maskFormat, op, &clip),
		   maskFormat && !operator_is_bounded(op));

skip:
	tor_fini(&tor);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &clip);
	return true;
}

static bool
triangles_mask_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			 PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			 int count, xTriangle *tri)
{
	struct tor tor;
	void (*span)(struct sna *sna,
		     struct sna_composite_spans_op *op,
		     pixman_region16_t *clip,
		     const BoxRec *box,
		     int coverage);
	ScreenPtr screen = dst->pDrawable->pScreen;
	PixmapPtr scratch;
	PicturePtr mask;
	BoxRec extents;
	int16_t dst_x, dst_y;
	int dx, dy;
	int error, n;

	if (NO_SCAN_CONVERTER)
		return false;

	if (dst->polyMode == PolyModePrecise) {
		DBG(("%s: fallback -- precise rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	if (maskFormat == NULL && count > 1) {
		DBG(("%s: fallback -- individual rasterisation requested\n",
		     __FUNCTION__));
		return false;
	}

	miTriangleBounds(count, tri, &extents);
	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

	if (!sna_compute_composite_extents(&extents,
					   src, NULL, dst,
					   src_x, src_y,
					   0, 0,
					   extents.x1, extents.y1,
					   extents.x2 - extents.x1,
					   extents.y2 - extents.y1))
		return true;

	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	extents.y2 -= extents.y1;
	extents.x2 -= extents.x1;
	extents.x1 -= dst->pDrawable->x;
	extents.y1 -= dst->pDrawable->y;
	dst_x = extents.x1;
	dst_y = extents.y1;
	dx = -extents.x1 * FAST_SAMPLES_X;
	dy = -extents.y1 * FAST_SAMPLES_Y;
	extents.x1 = extents.y1 = 0;

	DBG(("%s: mask (%dx%d)\n",
	     __FUNCTION__, extents.x2, extents.y2));
	scratch = sna_pixmap_create_upload(screen, extents.x2, extents.y2, 8);
	if (!scratch)
		return true;

	DBG(("%s: created buffer %p, stride %d\n",
	     __FUNCTION__, scratch->devPrivate.ptr, scratch->devKind));

	if (tor_init(&tor, &extents, 3*count)) {
		screen->DestroyPixmap(scratch);
		return true;
	}

	for (n = 0; n < count; n++) {
		xTriangle t;

		if (!project_triangle_onto_grid(&tri[n], dx, dy, &t))
			continue;

		polygon_add_line(tor.polygon, &t.p1, &t.p2);
		polygon_add_line(tor.polygon, &t.p2, &t.p3);
		polygon_add_line(tor.polygon, &t.p3, &t.p1);
	}

	if (maskFormat ? maskFormat->depth < 8 : dst->polyEdge == PolyEdgeSharp)
		span = tor_blt_mask_mono;
	else
		span = tor_blt_mask;

	tor_render(NULL, &tor,
		   scratch->devPrivate.ptr,
		   (void *)(intptr_t)scratch->devKind,
		   span, true);

	mask = CreatePicture(0, &scratch->drawable,
			     PictureMatchFormat(screen, 8, PICT_a8),
			     0, 0, serverClient, &error);
	screen->DestroyPixmap(scratch);
	if (mask) {
		CompositePicture(op, src, mask, dst,
				 src_x + dst_x - pixman_fixed_to_int(tri[0].p1.x),
				 src_y + dst_y - pixman_fixed_to_int(tri[0].p1.y),
				 0, 0,
				 dst_x, dst_y,
				 extents.x2, extents.y2);
		FreePicture(mask, 0);
	}
	tor_fini(&tor);

	return true;
}

static void
triangles_fallback(CARD8 op,
		   PicturePtr src,
		   PicturePtr dst,
		   PictFormatPtr maskFormat,
		   INT16 xSrc, INT16 ySrc,
		   int n, xTriangle *tri)
{
	ScreenPtr screen = dst->pDrawable->pScreen;

	DBG(("%s op=%d, count=%d\n", __FUNCTION__, op, n));

	if (maskFormat) {
		PixmapPtr scratch;
		PicturePtr mask;
		INT16 dst_x, dst_y;
		BoxRec bounds;
		int width, height, depth;
		pixman_image_t *image;
		pixman_format_code_t format;
		int error;

		dst_x = pixman_fixed_to_int(tri[0].p1.x);
		dst_y = pixman_fixed_to_int(tri[0].p1.y);

		miTriangleBounds(n, tri, &bounds);
		DBG(("%s: bounds (%d, %d), (%d, %d)\n",
		     __FUNCTION__, bounds.x1, bounds.y1, bounds.x2, bounds.y2));

		if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
			return;

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
			pixman_add_triangles(image,
					     -bounds.x1, -bounds.y1,
					     n, (pixman_triangle_t *)tri);
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

		for (; n--; tri++)
			triangles_fallback(op,
					   src, dst, maskFormat,
					   xSrc, ySrc, 1, tri);
	}
}

void
sna_composite_triangles(CARD8 op,
			 PicturePtr src,
			 PicturePtr dst,
			 PictFormatPtr maskFormat,
			 INT16 xSrc, INT16 ySrc,
			 int n, xTriangle *tri)
{
	if (triangles_span_converter(op, src, dst, maskFormat,
				     xSrc, ySrc,
				     n, tri))
		return;

	if (triangles_mask_converter(op, src, dst, maskFormat,
				     xSrc, ySrc,
				     n, tri))
		return;

	triangles_fallback(op, src, dst, maskFormat, xSrc, ySrc, n, tri);
}

static bool
tristrip_span_converter(CARD8 op, PicturePtr src, PicturePtr dst,
			PictFormatPtr maskFormat, INT16 src_x, INT16 src_y,
			int count, xPointFixed *points)
{
	struct sna *sna;
	struct sna_composite_spans_op tmp;
	struct tor tor;
	BoxRec extents;
	pixman_region16_t clip;
	xPointFixed p[4];
	int16_t dst_x, dst_y;
	int dx, dy;
	int cw, ccw, n;

	if (NO_SCAN_CONVERTER)
		return false;

	/* XXX strict adherence to the Render specification */
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

	dst_x = pixman_fixed_to_int(points[0].x);
	dst_y = pixman_fixed_to_int(points[0].y);

	miPointFixedBounds(count, points, &extents);
	DBG(("%s: extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, extents.x1, extents.y1, extents.x2, extents.y2));

	if (extents.y1 >= extents.y2 || extents.x1 >= extents.x2)
		return true;

#if 0
	if (extents.y2 - extents.y1 < 64 && extents.x2 - extents.x1 < 64) {
		DBG(("%s: fallback -- traps extents too small %dx%d\n",
		     __FUNCTION__, extents.y2 - extents.y1, extents.x2 - extents.x1));
		return false;
	}
#endif

	if (!sna_compute_composite_region(&clip,
					  src, NULL, dst,
					  src_x + extents.x1 - dst_x,
					  src_y + extents.y1 - dst_y,
					  0, 0,
					  extents.x1, extents.y1,
					  extents.x2 - extents.x1,
					  extents.y2 - extents.y1)) {
		DBG(("%s: triangles do not intersect drawable clips\n",
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
					 0,
					 &tmp)) {
		DBG(("%s: fallback -- composite spans render op not supported\n",
		     __FUNCTION__));
		return false;
	}

	dx *= FAST_SAMPLES_X;
	dy *= FAST_SAMPLES_Y;
	if (tor_init(&tor, &extents, 2*count))
		goto skip;

	cw = ccw = 0;
	project_point_onto_grid(&points[0], dx, dy, &p[cw]);
	project_point_onto_grid(&points[1], dx, dy, &p[2+ccw]);
	polygon_add_line(tor.polygon, &p[cw], &p[2+ccw]);
	n = 2;
	do {
		cw = !cw;
		project_point_onto_grid(&points[n], dx, dy, &p[cw]);
		polygon_add_line(tor.polygon, &p[!cw], &p[cw]);
		if (++n == count)
			break;

		ccw = !ccw;
		project_point_onto_grid(&points[n], dx, dy, &p[2+ccw]);
		polygon_add_line(tor.polygon, &p[2+ccw], &p[2+!ccw]);
		if (++n == count)
			break;
	} while (1);
	polygon_add_line(tor.polygon, &p[2+ccw], &p[cw]);
	assert(tor.polygon->num_edges <= 2*count);

	tor_render(sna, &tor, &tmp, &clip,
		   choose_span(dst, maskFormat, op, &clip),
		   maskFormat && !operator_is_bounded(op));

skip:
	tor_fini(&tor);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &clip);
	return true;
}

static void
tristrip_fallback(CARD8 op,
		  PicturePtr src,
		  PicturePtr dst,
		  PictFormatPtr maskFormat,
		  INT16 xSrc, INT16 ySrc,
		  int n, xPointFixed *points)
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

		dst_x = pixman_fixed_to_int(points->x);
		dst_y = pixman_fixed_to_int(points->y);

		miPointFixedBounds(n, points, &bounds);
		DBG(("%s: bounds (%d, %d), (%d, %d)\n",
		     __FUNCTION__, bounds.x1, bounds.y1, bounds.x2, bounds.y2));

		if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
			return;

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
			xTriangle tri;
			xPointFixed *p[3] = { &tri.p1, &tri.p2, &tri.p3 };
			int i;

			*p[0] = points[0];
			*p[1] = points[1];
			*p[2] = points[2];
			pixman_add_triangles(image,
					     -bounds.x1, -bounds.y1,
					     1, (pixman_triangle_t *)&tri);
			for (i = 3; i < n; i++) {
				*p[i%3] = points[i];
				pixman_add_triangles(image,
						     -bounds.x1, -bounds.y1,
						     1, (pixman_triangle_t *)&tri);
			}
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
		xTriangle tri;
		xPointFixed *p[3] = { &tri.p1, &tri.p2, &tri.p3 };
		int i;

		if (dst->polyEdge == PolyEdgeSharp)
			maskFormat = PictureMatchFormat(screen, 1, PICT_a1);
		else
			maskFormat = PictureMatchFormat(screen, 8, PICT_a8);

		*p[0] = points[0];
		*p[1] = points[1];
		*p[2] = points[2];
		triangles_fallback(op,
				   src, dst, maskFormat,
				   xSrc, ySrc, 1, &tri);
		for (i = 3; i < n; i++) {
			*p[i%3] = points[i];
			/* Should xSrc,ySrc be updated? */
			triangles_fallback(op,
					   src, dst, maskFormat,
					   xSrc, ySrc, 1, &tri);
		}
	}
}

void
sna_composite_tristrip(CARD8 op,
		       PicturePtr src,
		       PicturePtr dst,
		       PictFormatPtr maskFormat,
		       INT16 xSrc, INT16 ySrc,
		       int n, xPointFixed *points)
{
	if (tristrip_span_converter(op, src, dst, maskFormat, xSrc, ySrc, n, points))
		return;

	tristrip_fallback(op, src, dst, maskFormat, xSrc, ySrc, n, points);
}

static void
trifan_fallback(CARD8 op,
		PicturePtr src,
		PicturePtr dst,
		PictFormatPtr maskFormat,
		INT16 xSrc, INT16 ySrc,
		int n, xPointFixed *points)
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

		dst_x = pixman_fixed_to_int(points->x);
		dst_y = pixman_fixed_to_int(points->y);

		miPointFixedBounds(n, points, &bounds);
		DBG(("%s: bounds (%d, %d), (%d, %d)\n",
		     __FUNCTION__, bounds.x1, bounds.y1, bounds.x2, bounds.y2));

		if (bounds.y1 >= bounds.y2 || bounds.x1 >= bounds.x2)
			return;

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
			xTriangle tri;
			xPointFixed *p[3] = { &tri.p1, &tri.p2, &tri.p3 };
			int i;

			*p[0] = points[0];
			*p[1] = points[1];
			*p[2] = points[2];
			pixman_add_triangles(image,
					     -bounds.x1, -bounds.y1,
					     1, (pixman_triangle_t *)&tri);
			for (i = 3; i < n; i++) {
				*p[1+ (i%2)] = points[i];
				pixman_add_triangles(image,
						     -bounds.x1, -bounds.y1,
						     1, (pixman_triangle_t *)&tri);
			}
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
		xTriangle tri;
		xPointFixed *p[3] = { &tri.p1, &tri.p2, &tri.p3 };
		int i;

		if (dst->polyEdge == PolyEdgeSharp)
			maskFormat = PictureMatchFormat(screen, 1, PICT_a1);
		else
			maskFormat = PictureMatchFormat(screen, 8, PICT_a8);

		*p[0] = points[0];
		*p[1] = points[1];
		*p[2] = points[2];
		triangles_fallback(op,
				   src, dst, maskFormat,
				   xSrc, ySrc, 1, &tri);
		for (i = 3; i < n; i++) {
			*p[1 + (i%2)] = points[i];
			/* Should xSrc,ySrc be updated? */
			triangles_fallback(op,
					   src, dst, maskFormat,
					   xSrc, ySrc, 1, &tri);
		}
	}
}

void
sna_composite_trifan(CARD8 op,
		     PicturePtr src,
		     PicturePtr dst,
		     PictFormatPtr maskFormat,
		     INT16 xSrc, INT16 ySrc,
		     int n, xPointFixed *points)
{
	trifan_fallback(op, src, dst, maskFormat, xSrc, ySrc, n, points);
}
