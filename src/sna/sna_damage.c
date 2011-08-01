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
	enum mode {
		ADD,
		SUBTRACT,
	} mode;
	BoxPtr box;
	uint16_t n;
};

static struct sna_damage *_sna_damage_create(void)
{
	struct sna_damage *damage;

	damage = malloc(sizeof(*damage));
	damage->all = 0;
	damage->n = 0;
	damage->size = 16;
	damage->elts = malloc(sizeof(*damage->elts) * damage->size);
	list_init(&damage->boxes);
	damage->last_box = NULL;
	damage->mode = ADD;
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
		       enum mode mode,
		       const BoxRec *boxes, int count)
{
	struct sna_damage_elt *elt;

	DBG(("    %s(%s): n=%d, prev=(%s, remain %d)\n", __FUNCTION__,
	     mode == ADD ? "add" : "subtract",
	     damage->n,
	     damage->n ? damage->elts[damage->n-1].mode == ADD ? "add" : "subtract" : "none",
	     damage->last_box ? damage->last_box->remain : 0));

	if (damage->last_box && damage->elts[damage->n-1].mode == mode) {
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
	elt->mode = mode;
	elt->n = count;
	elt->box = memcpy(_sna_damage_create_boxes(damage, count),
			  boxes, count * sizeof(BoxRec));
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
	int n, m, j;
	int nboxes;
	BoxPtr boxes;
	pixman_region16_t tmp, *region = &damage->region;

	DBG(("    reduce: before damage.n=%d region.n=%d\n",
	     damage->n, REGION_NUM_RECTS(region)));

	m = 0;
	nboxes = damage->elts[0].n;
	boxes = damage->elts[0].box;
	for (n = 1; n < damage->n; n++) {
		if (damage->elts[n].mode != damage->elts[m].mode) {
			if (!boxes) {
				boxes = malloc(sizeof(BoxRec)*nboxes);
				nboxes = 0;
				for (j = m; j < n; j++) {
					memcpy(boxes+nboxes,
					       damage->elts[j].box,
					       damage->elts[j].n*sizeof(BoxRec));
					nboxes += damage->elts[j].n;
				}
			}

			pixman_region_init_rects(&tmp, boxes, nboxes);
			if (damage->elts[m].mode == ADD)
				pixman_region_union(region, region, &tmp);
			else
				pixman_region_subtract(region, region, &tmp);
			pixman_region_fini(&tmp);

			if (boxes != damage->elts[m].box)
				free(boxes);

			m = n;
			boxes = damage->elts[n].box;
			nboxes = damage->elts[n].n;
		} else {
			boxes = NULL;
			nboxes += damage->elts[n].n;
		}
	}

	if (!boxes) {
		boxes = malloc(sizeof(BoxRec)*nboxes);
		nboxes = 0;
		for (j = m; j < n; j++) {
			memcpy(boxes+nboxes,
			       damage->elts[j].box,
			       damage->elts[j].n*sizeof(BoxRec));
			nboxes += damage->elts[j].n;
		}
	}

	pixman_region_init_rects(&tmp, boxes, nboxes);
	if (damage->elts[m].mode == ADD)
		pixman_region_union(region, region, &tmp);
	else
		pixman_region_subtract(region, region, &tmp);
	pixman_region_fini(&tmp);

	damage->extents = region->extents;

	if (boxes != damage->elts[m].box)
		free(boxes);

	damage->n = 0;
	free_list(&damage->boxes);
	damage->last_box = NULL;
	damage->mode = ADD;

	DBG(("    reduce: after region.n=%d\n", REGION_NUM_RECTS(region)));
}

inline static struct sna_damage *__sna_damage_add(struct sna_damage *damage,
						  RegionPtr region)
{
	if (!RegionNotEmpty(region))
		return damage;

	if (!damage)
		damage = _sna_damage_create();

	if (damage->mode == SUBTRACT)
		__sna_damage_reduce(damage);
	damage->mode = ADD;

	if (REGION_NUM_RECTS(&damage->region) <= 1) {
		pixman_region_union(&damage->region, &damage->region, region);
		damage->extents = damage->region.extents;
		return damage;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     &region->extents) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt(damage, ADD,
			       REGION_RECTS(region),
			       REGION_NUM_RECTS(region));

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

inline static struct sna_damage *__sna_damage_add_box(struct sna_damage *damage,
						      const BoxRec *box)
{
	if (box->y2 <= box->y1 || box->x2 <= box->x1)
		return damage;

	if (!damage)
		damage = _sna_damage_create();

	if (damage->mode == SUBTRACT)
		__sna_damage_reduce(damage);
	damage->mode = ADD;

	if (REGION_NUM_RECTS(&damage->region) == 0) {
		pixman_region_init_rects(&damage->region, box, 1);
		damage->extents = *box;
		return damage;
	}

	if (pixman_region_contains_rectangle(&damage->region,
					     (BoxPtr)box) == PIXMAN_REGION_IN)
		return damage;

	_sna_damage_create_elt(damage, ADD, box, 1);

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
		damage->all = 1;
	} else
		damage = _sna_damage_create();

	pixman_region_init_rect(&damage->region, 0, 0, width, height);
	damage->extents = damage->region.extents;
	damage->mode = ADD;
	damage->all = 1;

	return damage;
}

struct sna_damage *_sna_damage_is_all(struct sna_damage *damage,
				      int width, int height)
{
	BoxRec box;

	if (damage->mode == SUBTRACT)
		__sna_damage_reduce(damage);

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

	if (damage->n == 0) {
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
			return damage;
		}
	}

	damage->all = 0;
	damage->mode = SUBTRACT;
	_sna_damage_create_elt(damage, SUBTRACT,
			       REGION_RECTS(region),
			       REGION_NUM_RECTS(region));

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

	if (damage->n == 0) {
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
			return damage;
		}
	}

	damage->mode = SUBTRACT;
	_sna_damage_create_elt(damage, SUBTRACT, box, 1);

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
		return PIXMAN_REGION_OUT;;

	if (!sna_damage_maybe_contains_box(damage, box))
		return PIXMAN_REGION_OUT;

	if (damage->n)
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

static Bool _sna_damage_intersect(struct sna_damage *damage,
				  RegionPtr region, RegionPtr result)
{
	if (!damage)
		return FALSE;

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
