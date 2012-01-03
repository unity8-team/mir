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

struct sna_damage_box {
	struct list list;
	int size;
} __attribute__((packed));

static struct sna_damage *__freed_damage;

static inline bool region_is_singular(RegionRec *r)
{
	return r->data == NULL;
}

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

	if (damage->mode == DAMAGE_ALL) {
		snprintf(buf, max, "[[(%d, %d), (%d, %d)]: all]",
			 damage->extents.x1, damage->extents.y1,
			 damage->extents.x2, damage->extents.y2);
	} else {
		if (damage->dirty) {
			sprintf(damage_str, "%c[ ...]",
				damage->mode == DAMAGE_SUBTRACT ? '-' : '+');
		} else
			damage_str[0] = '\0';
		snprintf(buf, max, "[[(%d, %d), (%d, %d)]: %s %s]",
			 damage->extents.x1, damage->extents.y1,
			 damage->extents.x2, damage->extents.y2,
			 _debug_describe_region(region_str, str_max,
						&damage->region),
			 damage_str);
	}

	return buf;
}

#endif

static struct sna_damage_box *
reset_embedded_box(struct sna_damage *damage)
{
	damage->dirty = false;
	damage->box = damage->embedded_box.box;
	damage->embedded_box.size =
		damage->remain = ARRAY_SIZE(damage->embedded_box.box);
	list_init(&damage->embedded_box.list);

	return (struct sna_damage_box *)&damage->embedded_box;
}

static struct sna_damage *_sna_damage_create(void)
{
	struct sna_damage *damage;

	if (__freed_damage) {
		damage = __freed_damage;
		__freed_damage = NULL;
	} else {
		damage = malloc(sizeof(*damage));
		if (damage == NULL)
			return NULL;
	}
	reset_embedded_box(damage);
	damage->mode = DAMAGE_ADD;
	pixman_region_init(&damage->region);
	damage->extents.x1 = damage->extents.y1 = MAXSHORT;
	damage->extents.x2 = damage->extents.y2 = MINSHORT;

	return damage;
}

static bool _sna_damage_create_boxes(struct sna_damage *damage,
				     int count)
{
	struct sna_damage_box *box;
	int n;

	box = list_entry(damage->embedded_box.list.prev,
			 struct sna_damage_box,
			 list);
	n = 4*box->size;
	if (n < count)
		n = ALIGN(count, 64);

	DBG(("    %s(%d->%d): new\n", __FUNCTION__, count, n));

	box = malloc(sizeof(*box) + sizeof(BoxRec)*n);
	if (box == NULL)
		return false;

	list_add_tail(&box->list, &damage->embedded_box.list);

	box->size = damage->remain = n;
	damage->box = (BoxRec *)(box + 1);
	return true;
}

static struct sna_damage *
_sna_damage_create_elt(struct sna_damage *damage,
		       const BoxRec *boxes, int count)
{
	int n;

	DBG(("    %s: prev=(remain %d)\n", __FUNCTION__, damage->remain));

	damage->dirty = true;
	n = count;
	if (n > damage->remain)
		n = damage->remain;
	if (n) {
		memcpy(damage->box, boxes, n * sizeof(BoxRec));
		damage->box += n;
		damage->remain -= n;

		count -=n;
		boxes += n;
		if (count == 0)
			return damage;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	if (_sna_damage_create_boxes(damage, count)) {
		memcpy(damage->box, boxes, count * sizeof(BoxRec));
		damage->box += count;
		damage->remain -= count;
	}

	 return damage;
}

static void
_sna_damage_create_elt_from_boxes(struct sna_damage *damage,
				  const BoxRec *boxes, int count,
				  int16_t dx, int16_t dy)
{
	int i, n;

	DBG(("    %s: prev=(remain %d)\n", __FUNCTION__, damage->remain));

	n = count;
	if (n > damage->remain)
		n = damage->remain;
	if (n) {
		for (i = 0; i < n; i++) {
			damage->box[i].x1 = boxes[i].x1 + dx;
			damage->box[i].x2 = boxes[i].x2 + dx;
			damage->box[i].y1 = boxes[i].y1 + dy;
			damage->box[i].y2 = boxes[i].y2 + dy;
		}
		damage->box += n;
		damage->remain -= n;

		count -=n;
		boxes += n;
		if (count == 0)
			return;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	if (!_sna_damage_create_boxes(damage, count))
		return;

	for (i = 0; i < count; i++) {
		damage->box[i].x1 = boxes[i].x1 + dx;
		damage->box[i].x2 = boxes[i].x2 + dx;
		damage->box[i].y1 = boxes[i].y1 + dy;
		damage->box[i].y2 = boxes[i].y2 + dy;
	}
	damage->box += i;
	damage->remain -= i;
}

static void
_sna_damage_create_elt_from_rectangles(struct sna_damage *damage,
				       const xRectangle *r, int count,
				       int16_t dx, int16_t dy)
{
	int i, n;

	DBG(("    %s: prev=(remain %d)\n", __FUNCTION__, damage->remain));

	n = count;
	if (n > damage->remain)
		n = damage->remain;
	if (n) {
		for (i = 0; i < n; i++) {
			damage->box[i].x1 = r[i].x + dx;
			damage->box[i].x2 = damage->box[i].x1 + r[i].width;
			damage->box[i].y1 = r[i].y + dy;
			damage->box[i].y2 = damage->box[i].y1 + r[i].height;
		}
		damage->box += n;
		damage->remain -= n;

		count -=n;
		r += n;
		if (count == 0)
			return;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	if (!_sna_damage_create_boxes(damage, count))
		return;

	for (i = 0; i < count; i++) {
		damage->box[i].x1 = r[i].x + dx;
		damage->box[i].x2 = damage->box[i].x1 + r[i].width;
		damage->box[i].y1 = r[i].y + dy;
		damage->box[i].y2 = damage->box[i].y1 + r[i].height;
	}
	damage->box += n;
	damage->remain -= n;
}

static void
_sna_damage_create_elt_from_points(struct sna_damage *damage,
				   const DDXPointRec *p, int count,
				   int16_t dx, int16_t dy)
{
	int i, n;

	DBG(("    %s: prev=(remain %d)\n", __FUNCTION__, damage->remain));

	n = count;
	if (n > damage->remain)
		n = damage->remain;
	if (n) {
		for (i = 0; i < n; i++) {
			damage->box[i].x1 = p[i].x + dx;
			damage->box[i].x2 = damage->box[i].x1 + 1;
			damage->box[i].y1 = p[i].y + dy;
			damage->box[i].y2 = damage->box[i].y1 + 1;
		}
		damage->box += n;
		damage->remain -= n;

		count -=n;
		p += n;
		if (count == 0)
			return;
	}

	DBG(("    %s(): new elt\n", __FUNCTION__));

	if (! _sna_damage_create_boxes(damage, count))
		return;

	for (i = 0; i < count; i++) {
		damage->box[i].x1 = p[i].x + dx;
		damage->box[i].x2 = damage->box[i].x1 + 1;
		damage->box[i].y1 = p[i].y + dy;
		damage->box[i].y2 = damage->box[i].y1 + 1;
	}
	damage->box += count;
	damage->remain -= count;
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
	BoxPtr boxes, free_boxes = NULL;
	pixman_region16_t *region = &damage->region;
	struct sna_damage_box *iter;

	assert(damage->mode != DAMAGE_ALL);
	assert(damage->dirty);

	DBG(("    reduce: before region.n=%d\n", REGION_NUM_RECTS(region)));

	nboxes = damage->embedded_box.size;
	list_for_each_entry(iter, &damage->embedded_box.list, list)
		nboxes += iter->size;
	DBG(("   nboxes=%d, residual=%d\n", nboxes, damage->remain));
	nboxes -= damage->remain;
	if (nboxes == 0)
		goto done;
	if (damage->mode == DAMAGE_ADD)
		nboxes += REGION_NUM_RECTS(region);

	iter = list_entry(damage->embedded_box.list.prev,
			  struct sna_damage_box,
			  list);
	n = iter->size - damage->remain;
	boxes = (BoxRec *)(iter+1);
	if (nboxes > iter->size) {
		boxes = malloc(sizeof(BoxRec)*nboxes);
		if (boxes == NULL)
			goto done;

		free_boxes = boxes;
	}

	if (boxes != damage->embedded_box.box) {
		if (list_is_empty(&damage->embedded_box.list)) {
			memcpy(boxes,
			       damage->embedded_box.box,
			       n*sizeof(BoxRec));
		} else {
			if (damage->mode == DAMAGE_ADD)
				nboxes -= REGION_NUM_RECTS(region);

			memcpy(boxes,
			       damage->embedded_box.box,
			       sizeof(damage->embedded_box.box));
			n = damage->embedded_box.size;

			list_for_each_entry(iter, &damage->embedded_box.list, list) {
				int len = iter->size;
				if (n + len > nboxes)
					len = nboxes - n;
				DBG(("   copy %d/%d boxes from %d\n", len, iter->size, n));
				memcpy(boxes + n, iter+1, len * sizeof(BoxRec));
				n += len;
			}

			if (damage->mode == DAMAGE_ADD)
				nboxes += REGION_NUM_RECTS(region);
		}
	}

	if (damage->mode == DAMAGE_ADD) {
		memcpy(boxes + n,
		       REGION_RECTS(region),
		       REGION_NUM_RECTS(region)*sizeof(BoxRec));
		assert(n + REGION_NUM_RECTS(region) == nboxes);
		pixman_region_fini(region);
		pixman_region_init_rects(region, boxes, nboxes);
	} else {
		pixman_region16_t tmp;

		pixman_region_init_rects(&tmp, boxes, nboxes);
		pixman_region_subtract(region, region, &tmp);
		pixman_region_fini(&tmp);
	}

	if (free_boxes)
		free(boxes);

	damage->extents = region->extents;

done:
	damage->mode = DAMAGE_ADD;
	free_list(&damage->embedded_box.list);
	reset_embedded_box(damage);

	DBG(("    reduce: after region.n=%d\n", REGION_NUM_RECTS(region)));
}

inline static struct sna_damage *__sna_damage_add(struct sna_damage *damage,
						  RegionPtr region)
{
	assert(RegionNotEmpty(region));

	if (!damage) {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	} else switch (damage->mode) {
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


	if (damage->extents.x1 > region->extents.x1)
		damage->extents.x1 = region->extents.x1;
	if (damage->extents.x2 < region->extents.x2)
		damage->extents.x2 = region->extents.x2;

	if (damage->extents.y1 > region->extents.y1)
		damage->extents.y1 = region->extents.y1;
	if (damage->extents.y2 < region->extents.y2)
		damage->extents.y2 = region->extents.y2;

	return _sna_damage_create_elt(damage,
				      REGION_RECTS(region),
				      REGION_NUM_RECTS(region));
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

	if (!damage) {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	} else switch (damage->mode) {
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
		damage->region.extents = box[0];
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

	if (!damage) {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	} else switch (damage->mode) {
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
		damage->region.extents.x1 = r[0].x + dx;
		damage->region.extents.x2 = r[0].x + r[0].width + dx;
		damage->region.extents.y1 = r[0].y + dy;
		damage->region.extents.y2 = r[0].y + r[0].height + dy;
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

	if (!damage) {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	} else switch (damage->mode) {
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
		damage->region.extents.x1 = p[0].x + dx;
		damage->region.extents.x2 = p[0].x + dx + 1;
		damage->region.extents.y1 = p[0].y + dy;
		damage->region.extents.y2 = p[0].y + dy + 1;
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

	if (!damage) {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	} else switch (damage->mode) {
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

	if (damage->extents.x1 > box->x1)
		damage->extents.x1 = box->x1;
	if (damage->extents.x2 < box->x2)
		damage->extents.x2 = box->x2;

	if (damage->extents.y1 > box->y1)
		damage->extents.y1 = box->y1;
	if (damage->extents.y2 < box->y2)
		damage->extents.y2 = box->y2;

	return _sna_damage_create_elt(damage, box, 1);
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
		pixman_region_fini(&damage->region);
		free_list(&damage->embedded_box.list);
		reset_embedded_box(damage);
	} else {
		damage = _sna_damage_create();
		if (damage == NULL)
			return NULL;
	}

	pixman_region_init_rect(&damage->region, 0, 0, width, height);
	damage->extents = damage->region.extents;
	damage->mode = DAMAGE_ALL;

	return damage;
}

struct sna_damage *_sna_damage_is_all(struct sna_damage *damage,
				      int width, int height)
{
	if (damage->dirty) {
		__sna_damage_reduce(damage);
		if (!RegionNotEmpty(&damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}
	}

	if (damage->region.data)
		return damage;

	assert(damage->extents.x1 == 0 &&
	       damage->extents.y1 == 0 &&
	       damage->extents.x2 == width &&
	       damage->extents.y2 == height);

	return _sna_damage_all(damage, width, height);
}

static bool box_contains(const BoxRec *a, const BoxRec *b)
{
	if (b->x1 < a->x1 || b->x2 > a->x2)
		return false;

	if (b->y1 < a->y1 || b->y2 > a->y2)
		return false;

	return true;
}

static inline Bool sna_damage_maybe_contains_box(const struct sna_damage *damage,
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

	assert(RegionNotEmpty(region));

	if (!sna_damage_maybe_contains_box(damage, &region->extents))
		return damage;

	assert(RegionNotEmpty(&damage->region));

	if (region_is_singular(region) &&
	    box_contains(&region->extents, &damage->extents)) {
		__sna_damage_destroy(damage);
		return NULL;
	}

	if (damage->mode == DAMAGE_ALL) {
		pixman_region_subtract(&damage->region,
				       &damage->region,
				       region);
		damage->extents = damage->region.extents;
		damage->mode = DAMAGE_ADD;
		return damage;
	}

	if (damage->mode != DAMAGE_SUBTRACT) {
		if (damage->dirty)
			__sna_damage_reduce(damage);

		if (pixman_region_equal(region, &damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (!pixman_region_not_empty(&damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (region_is_singular(&damage->region) &&
		    region_is_singular(region)) {
			pixman_region_subtract(&damage->region,
					       &damage->region,
					       region);
			damage->extents = damage->region.extents;
			return damage;
		}

		damage->mode = DAMAGE_SUBTRACT;
	}

	return _sna_damage_create_elt(damage,
				      REGION_RECTS(region),
				      REGION_NUM_RECTS(region));
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

	if (!sna_damage_maybe_contains_box(damage, box))
		return damage;

	assert(RegionNotEmpty(&damage->region));

	if (box_contains(box, &damage->extents)) {
		__sna_damage_destroy(damage);
		return NULL;
	}

	if (damage->mode != DAMAGE_SUBTRACT) {
		if (damage->dirty)
			__sna_damage_reduce(damage);

		if (!pixman_region_not_empty(&damage->region)) {
			__sna_damage_destroy(damage);
			return NULL;
		}

		if (region_is_singular(&damage->region)) {
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

	return _sna_damage_create_elt(damage, box, 1);
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
	int ret;

	if (!damage)
		return PIXMAN_REGION_OUT;

	if (damage->mode == DAMAGE_ALL)
		return PIXMAN_REGION_IN;

	if (!sna_damage_maybe_contains_box(damage, box))
		return PIXMAN_REGION_OUT;

	ret = pixman_region_contains_rectangle(&damage->region, (BoxPtr)box);
	if (!damage->dirty)
		return ret;

	if (damage->mode == DAMAGE_ADD && ret == PIXMAN_REGION_IN)
		return ret;

	__sna_damage_reduce(damage);
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

bool sna_damage_contains_box__no_reduce(const struct sna_damage *damage,
					const BoxRec *box)
{
	int ret;

	assert(damage && damage->mode != DAMAGE_ALL);
	if (!sna_damage_maybe_contains_box(damage, box))
		return false;

	ret = pixman_region_contains_rectangle((RegionPtr)&damage->region,
					       (BoxPtr)box);
	return (!damage->dirty || damage->mode == DAMAGE_ADD) &&
		ret == PIXMAN_REGION_IN;
}

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

	if (damage->dirty)
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
	if (ret)
		ErrorF("  = %s\n",
		       _debug_describe_region(region_buf, sizeof(region_buf), result));
	else
		ErrorF("  = none\n");

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

	if (damage->dirty)
		__sna_damage_reduce(damage);

	*boxes = REGION_RECTS(&damage->region);
	return REGION_NUM_RECTS(&damage->region);
}

struct sna_damage *_sna_damage_reduce(struct sna_damage *damage)
{
	DBG(("%s\n", __FUNCTION__));

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
	free_list(&damage->embedded_box.list);

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
