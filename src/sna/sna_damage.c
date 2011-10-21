/**************************************************************************

Copyright (c) 2011 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_damage.h"

/*
 * sna_damage is a batching layer on top of the regular pixman_region_t.
 * It is required as the ever-growing accumulation of invidual small
 * damage regions is an O(n^2) operation. Instead the accumulation of a
 * batch can be done in closer to O(n.lgn), and so prevents abysmal
 * performance in x11perf -copywinwin10.
 *
 * As with the core of SNA, damage is handled modally. That is, it
 * accumulates whilst rendering and then subtracts during migration of the
 * pixmap from GPU to CPU or vice versa. As such we can track the current
 * mode globally and when that mode switches perform the update of the region
 * in a single operation.
 *
 * Furthermore, we can track whether the whole pixmap is damaged and so
 * cheapy discard no-ops.
 */

static struct sna_damage *__freed_damage;

#if DEBUG_DAMAGE
#undef DBG
#define DBG(x) ErrorF x

static const char *_debug_describe_region(char *buf, int max,
					  RegionPtr region)
{
	BoxPtr extents;
	BoxPtr box;
	int n;
	int len;

	if (region == NULL)
		return "nil";

	n = REGION_NUM_RECTS(region);
	if (n == 0)
		return "[0]";

	extents = REGION_EXTENTS(NULL, region);
	if (n == 1) {
		sprintf(buf,
			"[(%d, %d), (%d, %d)]",
			extents->x1, extents->y1,
			extents->x2, extents->y2);
		return buf;
	}

	len = sprintf(buf,
		      "[(%d, %d), (%d, %d) x %d: ",
		      extents->x1, extents->y1,
		      extents->x2, extents->y2,
		      n) + 3;
	max -= 2;
	box = REGION_RECTS(region);
	while (n--) {
		char tmp[80];
		int this;

		this = snprintf(tmp, sizeof(tmp),
				"((%d, %d), (%d, %d))%s",
				box->x1, box->y1,
				box->x2, box->y2,
				n ? ", ..." : "");
		box++;

		if (this > max - len)
			break;

		len -= 3;
		memcpy(buf + len, tmp, this);
		len += this;
	}
	buf[len++] = ']';
	buf[len] = '\0';
	return buf;
}

static const char *_debug_describe_damage(char *buf, int max,
					  struct sna_damage *damage)
{
	char damage_str[500], region_str[500];
	int str_max;

	if (damage == NULL)
		return "None";

	str_max = max/2 - 6;
	if (str_max > sizeof(damage_str))
		str_max = sizeof(damage_str);

	sprintf(damage_str, "[%d : ...]", damage->n);
	snprintf(buf, max, "[[(%d, %d), (%d, %d)]:  %s + %s]",
		 damage->extents.x1, damage->extents.y1,
		 damage->extents.x2, damage->extents.y2,
		 _debug_describe_region(region_str, str_max,
					&damage->region),
		 damage_str);

	return buf;
}

#endif

struct sna_damage_box {
	struct list list;
	uint16_t size, remain;
};

struct sna_damage_elt {
	BoxPtr box;
	uint16_t n;
};

static struct sna_damage *_sna_damage_create(void)
{
	struct sna_damage *damage;

	if (__freed_damage) {
		damage = __freed_damage;
		__freed_damage = NULL;
	} else
		damage = malloc(sizeof(*damage));
	damage->n = 0;
	damage->size = 16;
	damage->elts = malloc(sizeof(*damage->elts) * damage->size);
	list_init(&damage->boxes);
	damage->last_box = NULL;
	damage->mode = DAMAGE_ADD;
	pixman_region_init(&damage->region);
	damage->extents.x1 = damage->extents.y1 = MAXSHORT;
	damage->extents.x2 = damage->extents.y2 = MINSHORT;

	return damage;
}

static BoxPtr _sna_damage_create_boxes(struct sna_damage *damage,
				       int count)
{
	struct sna_damage_box *box;
	int n;

	if (damage->last_box && damage->last_box->remain >= count) {
		box = damage->last_box;
		n = box->size - box->remain;
		DBG(("    %s(%d): reuse last box, used=%d, remain=%d\n",
		     __FUNCTION__, count, n, box->remain));
		box->remain -= count;
		if (box->remain == 0)
			damage->last_box = NULL;
		return (BoxPtr)(box+1) + n;
	}

	n = ALIGN(count, 64);

	DBG(("    %s(%d->%d): new\n", __FUNCTION__, count, n));

	box = malloc(sizeof(*box) + sizeof(BoxRec)*n);
	box->size = n;
	box->remain = n - count;
	list_add(&box->list, &damage->boxes);

	damage->last_box = box;
	return (BoxPtr)(box+1);
}

static void
_sna_damage_create_elt(struct sna_damage *damage,
		       const BoxRec *boxes, int count)
{
	struct sna_damage_elt *elt;

	DBG(("    %s: n=%d, prev=(remain %d)\n", __FUNCTION__,
	     damage->n,
	     damage->last_box ? damage->last_box->remain : 0));

	if (damage->last_box) {
		int n;

		n = count;
		if (n > damage->last_box->remain)
			n = damage->last_box->remain;

		elt = damage->elts + damage->n-1;
		memcpy(elt->box + elt->n, boxes, n * sizeof(BoxRec));
		elt->n += n;
		damage->last_box->remain -= n;
		if (damage->last_box->remain == 0)
			damage->last_box = NULL;

		count -=n;
		boxes += n;
		if (count == 0)
			return;
	}

	if (damage->n == damage->size) {
		int newsize = damage->size * 2;
		struct sna_damage_elt *newelts = realloc(damage->elts,
							 newsize*sizeof(*elt));
		if (newelts == NULL)
			return;

		damage->elts = newelts;
		damage->size = newsize;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	elt = damage->elts + damage->n++;
	elt->n = count;
	elt->box = memcpy(_sna_damage_create_boxes(damage, count),
			  boxes, count * sizeof(BoxRec));
}

static void
_sna_damage_create_elt_from_boxes(struct sna_damage *damage,
				  const BoxRec *boxes, int count,
				  int16_t dx, int16_t dy)
{
	struct sna_damage_elt *elt;
	int i;

	DBG(("    %s: n=%d, prev=(remain %d)\n", __FUNCTION__,
	     damage->n,
	     damage->last_box ? damage->last_box->remain : 0));

	if (damage->last_box) {
		int n;
		BoxRec *b;

		n = count;
		if (n > damage->last_box->remain)
			n = damage->last_box->remain;

		elt = damage->elts + damage->n-1;
		b = elt->box + elt->n;
		for (i = 0; i < n; i++) {
			b[i].x1 = boxes[i].x1 + dx;
			b[i].x2 = boxes[i].x2 + dx;
			b[i].y1 = boxes[i].y1 + dy;
			b[i].y2 = boxes[i].y2 + dy;
		}
		elt->n += n;
		damage->last_box->remain -= n;
		if (damage->last_box->remain == 0)
			damage->last_box = NULL;

		count -=n;
		boxes += n;
		if (count == 0)
			return;
	}

	if (damage->n == damage->size) {
		int newsize = damage->size * 2;
		struct sna_damage_elt *newelts = realloc(damage->elts,
							 newsize*sizeof(*elt));
		if (newelts == NULL)
			return;

		damage->elts = newelts;
		damage->size = newsize;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	elt = damage->elts + damage->n++;
	elt->n = count;
	elt->box = _sna_damage_create_boxes(damage, count);
	for (i = 0; i < count; i++) {
		elt->box[i].x1 = boxes[i].x1 + dx;
		elt->box[i].x2 = boxes[i].x2 + dx;
		elt->box[i].y1 = boxes[i].y1 + dy;
		elt->box[i].y2 = boxes[i].y2 + dy;
	}
}

static void
_sna_damage_create_elt_from_rectangles(struct sna_damage *damage,
				       const xRectangle *r, int count,
				       int16_t dx, int16_t dy)
{
	struct sna_damage_elt *elt;
	int i;

	DBG(("    %s: n=%d, prev=(remain %d)\n", __FUNCTION__,
	     damage->n,
	     damage->last_box ? damage->last_box->remain : 0));

	if (damage->last_box) {
		int n;
		BoxRec *b;

		n = count;
		if (n > damage->last_box->remain)
			n = damage->last_box->remain;

		elt = damage->elts + damage->n-1;
		b = elt->box + elt->n;
		for (i = 0; i < n; i++) {
			b[i].x1 = r[i].x + dx;
			b[i].x2 = b[i].x1 + r[i].width;
			b[i].y1 = r[i].y + dy;
			b[i].y2 = b[i].y1 + r[i].height;
		}
		elt->n += n;
		damage->last_box->remain -= n;
		if (damage->last_box->remain == 0)
			damage->last_box = NULL;

		count -=n;
		r += n;
		if (count == 0)
			return;
	}

	if (damage->n == damage->size) {
		int newsize = damage->size * 2;
		struct sna_damage_elt *newelts = realloc(damage->elts,
							 newsize*sizeof(*elt));
		if (newelts == NULL)
			return;

		damage->elts = newelts;
		damage->size = newsize;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	elt = damage->elts + damage->n++;
	elt->n = count;
	elt->box = _sna_damage_create_boxes(damage, count);
	for (i = 0; i < count; i++) {
		elt->box[i].x1 = r[i].x + dx;
		elt->box[i].x2 = elt->box[i].x1 + r[i].width;
		elt->box[i].y1 = r[i].y + dy;
		elt->box[i].y2 = elt->box[i].y1 + r[i].height;
	}
}

static void
_sna_damage_create_elt_from_points(struct sna_damage *damage,
				   const DDXPointRec *p, int count,
				   int16_t dx, int16_t dy)
{
	struct sna_damage_elt *elt;
	int i;

	DBG(("    %s: n=%d, prev=(remain %d)\n", __FUNCTION__,
	     damage->n,
	     damage->last_box ? damage->last_box->remain : 0));

	if (damage->last_box) {
		int n;
		BoxRec *b;

		n = count;
		if (n > damage->last_box->remain)
			n = damage->last_box->remain;

		elt = damage->elts + damage->n-1;
		b = elt->box + elt->n;
		for (i = 0; i < n; i++) {
			b[i].x1 = p[i].x + dx;
			b[i].x2 = b[i].x1 + 1;
			b[i].y1 = p[i].y + dy;
			b[i].y2 = b[i].y1 + 1;
		}
		elt->n += n;
		damage->last_box->remain -= n;
		if (damage->last_box->remain == 0)
			damage->last_box = NULL;

		count -=n;
		p += n;
		if (count == 0)
			return;
	}

	if (damage->n == damage->size) {
		int newsize = damage->size * 2;
		struct sna_damage_elt *newelts = realloc(damage->elts,
							 newsize*sizeof(*elt));
		if (newelts == NULL)
			return;

		damage->elts = newelts;
		damage->size = newsize;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	elt = damage->elts + damage->n++;
	elt->n = count;
	elt->box = _sna_damage_create_boxes(damage, count);
	for (i = 0; i < count; i++) {
		elt->box[i].x1 = p[i].x + dx;
		elt->box[i].x2 = elt->box[i].x1 + 1;
		elt->box[i].y1 = p[i].y + dy;
		elt->box[i].y2 = elt->box[i].y1 + 1;
	}
}

static void free_list(struct list *head)
{
	while (!list_is_empty(head)) {
		struct list *l = head->next;
		list_del(l);
		free(l);
	}
}

static void __sna_damage_reduce(struct sna_damage *damage)
{
	int n, nboxes;
	BoxPtr boxes;
	pixman_region16_t tmp, *region = &damage->region;

	assert(damage->mode != DAMAGE_ALL);
	assert(damage->n);

	DBG(("    reduce: before damage.n=%d region.n=%d\n",
	     damage->n, REGION_NUM_RECTS(region)));

	nboxes = damage->elts[0].n;
	if (damage->n == 1) {
		boxes = damage->elts[0].box;
	} else {
		for (n = 1; n < damage->n; n++)
			nboxes += damage->elts[n].n;

		boxes = malloc(sizeof(BoxRec)*nboxes);
		nboxes = 0;
		for (n = 0; n < damage->n; n++) {
			memcpy(boxes+nboxes,
			       damage->elts[n].box,
			       damage->elts[n].n*sizeof(BoxRec));
			nboxes += damage->elts[n].n;
		}
	}

	pixman_region_init_rects(&tmp, boxes, nboxes);
	if (damage->mode == DAMAGE_ADD)
		pixman_region_union(region, region, &tmp);
	else
		pixman_region_subtract(region, region, &tmp);
	pixman_region_fini(&tmp);

	damage->extents = region->extents;

	if (boxes != damage->elts[0].box)
		free(boxes);

	damage->n = 0;
	damage->mode = DAMAGE_ADD;
	free_list(&damage->boxes);
	damage->last_box = NULL;

	DBG(("    reduce: after region.n=%d\n", REGION_NUM_RECTS(region)));
}

inline static struct sna_damage *__sna_damage_add(struct sna_damage *damage,
						  RegionPtr region)
{
	if (!RegionNotEmpty(region))
		return damage;

	if (!damage)
		damage = _sna_damage_create();
	else switch (damage->mode) {
	case DAMAGE_ALL:
		return damage;
	case DAMAGE_SUBTRACT:
		__sna_damage_reduce(damage);
	case DAMAGE_ADD:
		break;
	}

	if (REGION_NUM_RECTS(&damage->region) <= 1) {
		pixman_region_union(&damage->region, &damage->region, region);
		damage->extents = damage->region.extents;
		return damage;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     &region->extents) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt(damage,
			       REGION_RECTS(region), REGION_NUM_RECTS(region));

	if (damage->extents.x1 > region->extents.x1)
		damage->extents.x1 = region->extents.x1;
	if (damage->extents.x2 < region->extents.x2)
		damage->extents.x2 = region->extents.x2;

	if (damage->extents.y1 > region->extents.y1)
		damage->extents.y1 = region->extents.y1;
	if (damage->extents.y2 < region->extents.y2)
		damage->extents.y2 = region->extents.y2;

	return damage;
}

#if DEBUG_DAMAGE
fastcall struct sna_damage *_sna_damage_add(struct sna_damage *damage,
					    RegionPtr region)
{
	char region_buf[120];
	char damage_buf[1000];

	DBG(("%s(%s + %s)\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     _debug_describe_region(region_buf, sizeof(region_buf), region)));

	damage = __sna_damage_add(damage, region);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
fastcall struct sna_damage *_sna_damage_add(struct sna_damage *damage,
					    RegionPtr region)
{
	return __sna_damage_add(damage, region);
}
#endif

inline static struct sna_damage *
__sna_damage_add_boxes(struct sna_damage *damage,
		       const BoxRec *box, int n,
		       int16_t dx, int16_t dy)
{
	BoxRec extents;
	int i;

	assert(n);

	if (!damage)
		damage = _sna_damage_create();
	else switch (damage->mode) {
	case DAMAGE_ALL:
		return damage;
	case DAMAGE_SUBTRACT:
		__sna_damage_reduce(damage);
	case DAMAGE_ADD:
		break;
	}

	extents = box[0];
	for (i = 1; i < n; i++) {
		if (extents.x1 > box[i].x1)
			extents.x1 = box[i].x1;
		if (extents.x2 < box[i].x2)
			extents.x2 = box[i].x2;
		if (extents.y1 > box[i].y1)
			extents.y1 = box[i].y1;
		if (extents.y2 < box[i].y2)
			extents.y2 = box[i].y2;
	}

	assert(extents.y2 > extents.y1 && extents.x2 > extents.x1);

	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	if (pixman_region_contains_rectangle(&damage->region,
					     &extents) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt_from_boxes(damage, box, n, dx, dy);

	if (REGION_NUM_RECTS(&damage->region) == 0) {
		damage->region.extents = *damage->elts[0].box;
		damage->region.data = NULL;
		damage->extents = extents;
	} else {
		if (damage->extents.x1 > extents.x1)
			damage->extents.x1 = extents.x1;
		if (damage->extents.x2 < extents.x2)
			damage->extents.x2 = extents.x2;

		if (damage->extents.y1 > extents.y1)
			damage->extents.y1 = extents.y1;
		if (damage->extents.y2 < extents.y2)
			damage->extents.y2 = extents.y2;
	}

	return damage;
}

#if DEBUG_DAMAGE
struct sna_damage *_sna_damage_add_boxes(struct sna_damage *damage,
					 const BoxRec *b, int n,
					 int16_t dx, int16_t dy)
{
	char damage_buf[1000];

	DBG(("%s(%s + [(%d, %d), (%d, %d) ... x %d])\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     b->x1, b->y1, b->x2, b->y2, n));

	damage = __sna_damage_add_boxes(damage, b, n, dx, dy);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
struct sna_damage *_sna_damage_add_boxes(struct sna_damage *damage,
					 const BoxRec *b, int n,
					 int16_t dx, int16_t dy)
{
	return __sna_damage_add_boxes(damage, b, n, dx, dy);
}
#endif

static void _pixman_region_union_box(RegionRec *region, const BoxRec *box)
{
	RegionRec u = { *box, NULL };
	pixman_region_union(region, region, &u);
}

inline static struct sna_damage *
__sna_damage_add_rectangles(struct sna_damage *damage,
			    const xRectangle *r, int n,
			    int16_t dx, int16_t dy)
{
	BoxRec extents;
	int i;

	assert(n);

	extents.x1 = r[0].x;
	extents.x2 = r[0].x + r[0].width;
	extents.y1 = r[0].y;
	extents.y2 = r[0].y + r[0].height;
	for (i = 1; i < n; i++) {
		if (extents.x1 > r[i].x)
			extents.x1 = r[i].x;
		if (extents.x2 < r[i].x + r[i].width)
			extents.x2 = r[i].x + r[i].width;
		if (extents.y1 > r[i].y)
			extents.y1 = r[i].y;
		if (extents.y2 < r[i].y + r[i].height)
			extents.y2 = r[i].y + r[i].height;
	}

	assert(extents.y2 > extents.y1 && extents.x2 > extents.x1);

	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	if (!damage)
		damage = _sna_damage_create();
	else switch (damage->mode) {
	case DAMAGE_ALL:
		return damage;
	case DAMAGE_SUBTRACT:
		__sna_damage_reduce(damage);
	case DAMAGE_ADD:
		break;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     &extents) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt_from_rectangles(damage, r, n, dx, dy);

	if (REGION_NUM_RECTS(&damage->region) == 0) {
		damage->region.extents = *damage->elts[0].box;
		damage->region.data = NULL;
		damage->extents = extents;
	} else {
		if (damage->extents.x1 > extents.x1)
			damage->extents.x1 = extents.x1;
		if (damage->extents.x2 < extents.x2)
			damage->extents.x2 = extents.x2;

		if (damage->extents.y1 > extents.y1)
			damage->extents.y1 = extents.y1;
		if (damage->extents.y2 < extents.y2)
			damage->extents.y2 = extents.y2;
	}

	return damage;
}

#if DEBUG_DAMAGE
struct sna_damage *_sna_damage_add_rectangles(struct sna_damage *damage,
					      const xRectangle *r, int n,
					      int16_t dx, int16_t dy)
{
	char damage_buf[1000];

	DBG(("%s(%s + [(%d, %d)x(%d, %d) ... x %d])\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     r->x, r->y, r->width, r->height, n));

	damage = __sna_damage_add_rectangles(damage, r, n, dx, dy);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
struct sna_damage *_sna_damage_add_rectangles(struct sna_damage *damage,
					      const xRectangle *r, int n,
					      int16_t dx, int16_t dy)
{
	return __sna_damage_add_rectangles(damage, r, n, dx, dy);
}
#endif

/* XXX pass in extents? */
inline static struct sna_damage *
__sna_damage_add_points(struct sna_damage *damage,
			const DDXPointRec *p, int n,
			int16_t dx, int16_t dy)
{
	BoxRec extents;
	int i;

	assert(n);

	extents.x2 = extents.x1 = p[0].x;
	extents.y2 = extents.y1 = p[0].y;
	for (i = 1; i < n; i++) {
		if (extents.x1 > p[i].x)
			extents.x1 = p[i].x;
		else if (extents.x2 < p[i].x)
			extents.x2 = p[i].x;
		if (extents.y1 > p[i].y)
			extents.y1 = p[i].y;
		else if (extents.y2 < p[i].y)
			extents.y2 = p[i].y;
	}

	extents.x1 += dx;
	extents.x2 += dx + 1;
	extents.y1 += dy;
	extents.y2 += dy + 1;

	if (!damage)
		damage = _sna_damage_create();
	else switch (damage->mode) {
	case DAMAGE_ALL:
		return damage;
	case DAMAGE_SUBTRACT:
		__sna_damage_reduce(damage);
	case DAMAGE_ADD:
		break;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     &extents) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt_from_points(damage, p, n, dx, dy);

	if (REGION_NUM_RECTS(&damage->region) == 0) {
		damage->region.extents = *damage->elts[0].box;
		damage->region.data = NULL;
		damage->extents = extents;
	} else {
		if (damage->extents.x1 > extents.x1)
			damage->extents.x1 = extents.x1;
		if (damage->extents.x2 < extents.x2)
			damage->extents.x2 = extents.x2;

		if (damage->extents.y1 > extents.y1)
			damage->extents.y1 = extents.y1;
		if (damage->extents.y2 < extents.y2)
			damage->extents.y2 = extents.y2;
	}

	return damage;
}

#if DEBUG_DAMAGE
struct sna_damage *_sna_damage_add_points(struct sna_damage *damage,
					  const DDXPointRec *p, int n,
					  int16_t dx, int16_t dy)
{
	char damage_buf[1000];

	DBG(("%s(%s + [(%d, %d) ... x %d])\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     p->x, p->y, n));

	damage = __sna_damage_add_points(damage, p, n, dx, dy);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
struct sna_damage *_sna_damage_add_points(struct sna_damage *damage,
					  const DDXPointRec *p, int n,
					  int16_t dx, int16_t dy)
{
	return __sna_damage_add_points(damage, p, n, dx, dy);
}
#endif

inline static struct sna_damage *__sna_damage_add_box(struct sna_damage *damage,
						      const BoxRec *box)
{
	if (box->y2 <= box->y1 || box->x2 <= box->x1)
		return damage;

	if (!damage)
		damage = _sna_damage_create();
	else switch (damage->mode) {
	case DAMAGE_ALL:
		return damage;
	case DAMAGE_SUBTRACT:
		__sna_damage_reduce(damage);
	case DAMAGE_ADD:
		break;
	}

	switch (REGION_NUM_RECTS(&damage->region)) {
	case 0:
		pixman_region_init_rects(&damage->region, box, 1);
		damage->extents = *box;
		return damage;
	case 1:
		_pixman_region_union_box(&damage->region, box);
		damage->extents = damage->region.extents;
		return damage;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     (BoxPtr)box) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt(damage, box, 1);

	if (damage->extents.x1 > box->x1)
		damage->extents.x1 = box->x1;
	if (damage->extents.x2 < box->x2)
		damage->extents.x2 = box->x2;

	if (damage->extents.y1 > box->y1)
		damage->extents.y1 = box->y1;
	if (damage->extents.y2 < box->y2)
		damage->extents.y2 = box->y2;

	return damage;
}

#if DEBUG_DAMAGE
fastcall struct sna_damage *_sna_damage_add_box(struct sna_damage *damage,
						const BoxRec *box)
{
	char damage_buf[1000];

	DBG(("%s(%s + [(%d, %d), (%d, %d)])\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     box->x1, box->y1, box->x2, box->y2));

	damage = __sna_damage_add_box(damage, box);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
fastcall struct sna_damage *_sna_damage_add_box(struct sna_damage *damage,
						const BoxRec *box)
{
	return __sna_damage_add_box(damage, box);
}
#endif

struct sna_damage *_sna_damage_all(struct sna_damage *damage,
				   int width, int height)
{
	DBG(("%s(%d, %d)\n", __FUNCTION__, width, height));

	if (damage) {
		free_list(&damage->boxes);
		pixman_region_fini(&damage->region);
		damage->n = 0;
		damage->last_box = NULL;
	} else
		damage = _sna_damage_create();

	pixman_region_init_rect(&damage->region, 0, 0, width, height);
	damage->extents = damage->region.extents;
	damage->mode = DAMAGE_ALL;

	return damage;
}

struct sna_damage *_sna_damage_is_all(struct sna_damage *damage,
				      int width, int height)
{
	BoxRec box;

	box.x1 = box.y1 = 0;
	box.x2 = width;
	box.y2 = height;

	if (pixman_region_contains_rectangle(&damage->region,
					     &box) != PIXMAN_REGION_IN)
		return damage;

	return _sna_damage_all(damage, width, height);
}

static inline Bool sna_damage_maybe_contains_box(struct sna_damage *damage,
						 const BoxRec *box)
{
	if (box->x2 <= damage->extents.x1 ||
	    box->x1 >= damage->extents.x2)
		return FALSE;

	if (box->y2 <= damage->extents.y1 ||
	    box->y1 >= damage->extents.y2)
		return FALSE;

	return TRUE;
}

static struct sna_damage *__sna_damage_subtract(struct sna_damage *damage,
						RegionPtr region)
{
	if (damage == NULL)
		return NULL;

	if (!RegionNotEmpty(&damage->region)) {
		__sna_damage_destroy(damage);
		return NULL;
	}

	if (!RegionNotEmpty(region))
		return damage;

	if (!sna_damage_maybe_contains_box(damage, &region->extents))
		return damage;

	if (damage->mode != DAMAGE_SUBTRACT) {
		if (damage->n)
			__sna_damage_reduce(damage);

		if (pixman_region_equal(region, &damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (!pixman_region_not_empty(&damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (REGION_NUM_RECTS(&damage->region) == 1 &&
		    REGION_NUM_RECTS(region) == 1) {
			pixman_region_subtract(&damage->region,
					       &damage->region,
					       region);
			damage->extents = damage->region.extents;
			damage->mode = DAMAGE_ADD; /* reduce from ALL */
			return damage;
		}

		damage->mode = DAMAGE_SUBTRACT;
	}

	_sna_damage_create_elt(damage,
			       REGION_RECTS(region), REGION_NUM_RECTS(region));

	return damage;
}

#if DEBUG_DAMAGE
fastcall struct sna_damage *_sna_damage_subtract(struct sna_damage *damage,
						 RegionPtr region)
{
	char damage_buf[1000];
	char region_buf[120];

	ErrorF("%s(%s - %s)...\n", __FUNCTION__,
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	       _debug_describe_region(region_buf, sizeof(region_buf), region));

	damage = __sna_damage_subtract(damage, region);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
fastcall struct sna_damage *_sna_damage_subtract(struct sna_damage *damage,
						 RegionPtr region)
{
	return __sna_damage_subtract(damage, region);
}
#endif

inline static struct sna_damage *__sna_damage_subtract_box(struct sna_damage *damage,
							   const BoxRec *box)
{
	if (damage == NULL)
		return NULL;

	if (!RegionNotEmpty(&damage->region)) {
		__sna_damage_destroy(damage);
		return NULL;
	}

	if (!sna_damage_maybe_contains_box(damage, box))
		return damage;

	if (damage->mode != DAMAGE_SUBTRACT) {
		if (damage->n)
			__sna_damage_reduce(damage);

		if (!pixman_region_not_empty(&damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (REGION_NUM_RECTS(&damage->region) == 1) {
			pixman_region16_t region;

			pixman_region_init_rects(&region, box, 1);
			pixman_region_subtract(&damage->region,
					       &damage->region,
					       &region);
			damage->extents = damage->region.extents;
			damage->mode = DAMAGE_ADD;
			return damage;
		}

		damage->mode = DAMAGE_SUBTRACT;
	}

	_sna_damage_create_elt(damage, box, 1);
	return damage;
}

#if DEBUG_DAMAGE
fastcall struct sna_damage *_sna_damage_subtract_box(struct sna_damage *damage,
						     const BoxRec *box)
{
	char damage_buf[1000];

	ErrorF("%s(%s - (%d, %d), (%d, %d))...\n", __FUNCTION__,
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	       box->x1, box->y1, box->x2, box->y2);

	damage = __sna_damage_subtract_box(damage, box);

	ErrorF("  = %s\n",
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	return damage;
}
#else
fastcall struct sna_damage *_sna_damage_subtract_box(struct sna_damage *damage,
						     const BoxRec *box)
{
	return __sna_damage_subtract_box(damage, box);
}
#endif

static int _sna_damage_contains_box(struct sna_damage *damage,
				    const BoxRec *box)
{
	if (!damage)
		return PIXMAN_REGION_OUT;

	if (damage->mode == DAMAGE_ALL)
		return PIXMAN_REGION_IN;

	if (!sna_damage_maybe_contains_box(damage, box))
		return PIXMAN_REGION_OUT;

	if (damage->n) {
		if (damage->mode != DAMAGE_SUBTRACT) {
			int ret = pixman_region_contains_rectangle(&damage->region,
								   (BoxPtr)box);
			if (ret == PIXMAN_REGION_IN)
				return PIXMAN_REGION_IN;
		}

		__sna_damage_reduce(damage);
	}

	return pixman_region_contains_rectangle(&damage->region, (BoxPtr)box);
}

#if DEBUG_DAMAGE
int sna_damage_contains_box(struct sna_damage *damage,
			    const BoxRec *box)
{
	char damage_buf[1000];
	int ret;

	DBG(("%s(%s, [(%d, %d), (%d, %d)])\n", __FUNCTION__,
	     _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	     box->x1, box->y1, box->x2, box->y2));

	ret = _sna_damage_contains_box(damage, box);
	ErrorF("  = %d", ret);
	if (ret)
		ErrorF(" [(%d, %d), (%d, %d)...]",
		       box->x1, box->y1, box->x2, box->y2);
	ErrorF("\n");

	return ret;
}
#else
int sna_damage_contains_box(struct sna_damage *damage,
			    const BoxRec *box)
{
	return _sna_damage_contains_box(damage, box);
}
#endif

static Bool _sna_damage_intersect(struct sna_damage *damage,
				  RegionPtr region, RegionPtr result)
{
	if (!damage)
		return FALSE;

	if (damage->mode == DAMAGE_ALL) {
		RegionCopy(result, region);
		return TRUE;
	}

	if (region->extents.x2 <= damage->extents.x1 ||
	    region->extents.x1 >= damage->extents.x2)
		return FALSE;

	if (region->extents.y2 <= damage->extents.y1 ||
	    region->extents.y1 >= damage->extents.y2)
		return FALSE;

	if (damage->n)
		__sna_damage_reduce(damage);

	if (!pixman_region_not_empty(&damage->region))
		return FALSE;

	RegionNull(result);
	RegionIntersect(result, &damage->region, region);

	return RegionNotEmpty(result);
}

#if DEBUG_DAMAGE
Bool sna_damage_intersect(struct sna_damage *damage,
			  RegionPtr region, RegionPtr result)
{
	char damage_buf[1000];
	char region_buf[120];
	Bool ret;

	ErrorF("%s(%s, %s)...\n", __FUNCTION__,
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage),
	       _debug_describe_region(region_buf, sizeof(region_buf), region));

	ret = _sna_damage_intersect(damage, region, result);
	ErrorF("  = %d %s\n",
	       ret,
	       _debug_describe_region(region_buf, sizeof(region_buf), result));

	return ret;
}
#else
Bool sna_damage_intersect(struct sna_damage *damage,
			  RegionPtr region, RegionPtr result)
{
	return _sna_damage_intersect(damage, region, result);
}
#endif

static int _sna_damage_get_boxes(struct sna_damage *damage, BoxPtr *boxes)
{
	if (!damage)
		return 0;

	if (damage->n)
		__sna_damage_reduce(damage);

	*boxes = REGION_RECTS(&damage->region);
	return REGION_NUM_RECTS(&damage->region);
}

struct sna_damage *_sna_damage_reduce(struct sna_damage *damage)
{
	DBG(("%s()\n", __FUNCTION__));

	if (damage->n)
		__sna_damage_reduce(damage);

	if (!pixman_region_not_empty(&damage->region)) {
		__sna_damage_destroy(damage);
		damage = NULL;
	}

	return damage;
}

#if DEBUG_DAMAGE
int sna_damage_get_boxes(struct sna_damage *damage, BoxPtr *boxes)
{
	char damage_buf[1000];
	int count;

	ErrorF("%s(%s)...\n", __FUNCTION__,
	       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));

	count = _sna_damage_get_boxes(damage, boxes);
	ErrorF("  = %d\n", count);

	return count;
}
#else
int sna_damage_get_boxes(struct sna_damage *damage, BoxPtr *boxes)
{
	return _sna_damage_get_boxes(damage, boxes);
}
#endif

void __sna_damage_destroy(struct sna_damage *damage)
{
	free(damage->elts);

	free_list(&damage->boxes);

	pixman_region_fini(&damage->region);
	if (__freed_damage == NULL)
		__freed_damage = damage;
	else
		free(damage);
}

#if DEBUG_DAMAGE && TEST_DAMAGE
struct sna_damage_selftest{
	int width, height;
};

static void st_damage_init_random_box(struct sna_damage_selftest *test,
				      BoxPtr box)
{
	int x, y, w, h;

	if (test->width == 1) {
		x = 0, w = 1;
	} else {
		x = rand() % (test->width - 1);
		w = 1 + rand() % (test->width - x - 1);
	}

	if (test->height == 1) {
		y = 0, h = 1;
	} else {
		y = rand() % (test->height - 1);
		h = 1 + rand() % (test->height - y - 1);
	}

	box->x1 = x;
	box->x2 = x+w;

	box->y1 = y;
	box->y2 = y+h;
}

static void st_damage_init_random_region1(struct sna_damage_selftest *test,
					  pixman_region16_t *region)
{
	int x, y, w, h;

	if (test->width == 1) {
		x = 0, w = 1;
	} else {
		x = rand() % (test->width - 1);
		w = 1 + rand() % (test->width - x - 1);
	}

	if (test->height == 1) {
		y = 0, h = 1;
	} else {
		y = rand() % (test->height - 1);
		h = 1 + rand() % (test->height - y - 1);
	}

	pixman_region_init_rect(region, x, y, w, h);
}

static void st_damage_add(struct sna_damage_selftest *test,
			  struct sna_damage **damage,
			  pixman_region16_t *region)
{
	pixman_region16_t tmp;

	st_damage_init_random_region1(test, &tmp);

	sna_damage_add(damage, &tmp);
	pixman_region_union(region, region, &tmp);
}

static void st_damage_add_box(struct sna_damage_selftest *test,
			      struct sna_damage **damage,
			      pixman_region16_t *region)
{
	BoxRec box;

	st_damage_init_random_box(test, &box);

	sna_damage_add_box(damage, &box);
	pixman_region_union_rectangle(region, region,
				      box.x1, box.y2,
				      box.x2 - box.x1,
				      box.y2 - box.y1);
}

static void st_damage_subtract(struct sna_damage_selftest *test,
			       struct sna_damage **damage,
			       pixman_region16_t *region)
{
	pixman_region16_t tmp;

	st_damage_init_random_region1(test, &tmp);

	sna_damage_subtract(damage, &tmp);
	pixman_region_subtract(region, region, &tmp);
}

static void st_damage_all(struct sna_damage_selftest *test,
			  struct sna_damage **damage,
			  pixman_region16_t *region)
{
	pixman_region16_t tmp;

	pixman_region_init_rect(&tmp, 0, 0, test->width, test->height);

	sna_damage_all(damage, test->width, test->height);
	pixman_region_union(region, region, &tmp);
}

static bool st_check_equal(struct sna_damage_selftest *test,
			   struct sna_damage **damage,
			   pixman_region16_t *region)
{
	int d_num, r_num;
	BoxPtr d_boxes, r_boxes;

	d_num = sna_damage_get_boxes(*damage, &d_boxes);
	r_boxes = pixman_region_rectangles(region, &r_num);

	if (d_num != r_num) {
		ErrorF("%s: damage and ref contain different number of rectangles\n",
		       __FUNCTION__);
		return FALSE;
	}

	if (memcmp(d_boxes, r_boxes, d_num*sizeof(BoxRec))) {
		ErrorF("%s: damage and ref contain different rectangles\n",
		       __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

void sna_damage_selftest(void)
{
	void (*const op[])(struct sna_damage_selftest *test,
			   struct sna_damage **damage,
			   pixman_region16_t *region) = {
		st_damage_add,
		st_damage_add_box,
		st_damage_subtract,
		st_damage_all
	};
	bool (*const check[])(struct sna_damage_selftest *test,
			      struct sna_damage **damage,
			      pixman_region16_t *region) = {
		st_check_equal,
		//st_check_contains,
	};
	char region_buf[120];
	char damage_buf[1000];
	int pass;

	for (pass = 0; pass < 1024; pass++) {
		struct sna_damage_selftest test;
		struct sna_damage *damage;
		pixman_region16_t ref;
		int iter, i;

		iter = rand() % 1024;

		test.width = 1 + rand() % 2048;
		test.height = 1 + rand() % 2048;

		damage = _sna_damage_create();
		pixman_region_init(&ref);

		for (i = 0; i < iter; i++) {
			op[rand() % ARRAY_SIZE(op)](&test, &damage, &ref);
		}

		if (!check[rand() % ARRAY_SIZE(check)](&test, &damage, &ref)) {
			ErrorF("%s: failed - region = %s, damage = %s\n", __FUNCTION__,
			       _debug_describe_region(region_buf, sizeof(region_buf), &ref),
			       _debug_describe_damage(damage_buf, sizeof(damage_buf), damage));
			assert(0);
		}

		pixman_region_fini(&ref);
		sna_damage_destroy(&damage);
	}
}
#endif
