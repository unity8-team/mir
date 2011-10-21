#ifndef SNA_DAMAGE_H
#define SNA_DAMAGE_H

#include <regionstr.h>
#include <list.h>

#include "compiler.h"

struct sna_damage_elt;
struct sna_damage_box;

struct sna_damage {
	BoxRec extents;
	pixman_region16_t region;
	enum sna_damage_mode {
		DAMAGE_ADD = 0,
		DAMAGE_SUBTRACT,
		DAMAGE_ALL,
	} mode;
	int n, size;
	struct sna_damage_elt *elts;
	struct sna_damage_box *last_box;
	struct list boxes;
};

fastcall struct sna_damage *_sna_damage_add(struct sna_damage *damage,
					    RegionPtr region);
static inline void sna_damage_add(struct sna_damage **damage,
				  RegionPtr region)
{
	*damage = _sna_damage_add(*damage, region);
}

fastcall struct sna_damage *_sna_damage_add_box(struct sna_damage *damage,
						const BoxRec *box);
static inline void sna_damage_add_box(struct sna_damage **damage,
				      const BoxRec *box)
{
	*damage = _sna_damage_add_box(*damage, box);
}

struct sna_damage *_sna_damage_add_boxes(struct sna_damage *damage,
					 const BoxRec *box, int n,
					 int16_t dx, int16_t dy);
static inline void sna_damage_add_boxes(struct sna_damage **damage,
					const BoxRec *box, int n,
					int16_t dx, int16_t dy)
{
	*damage = _sna_damage_add_boxes(*damage, box, n, dx, dy);
}

struct sna_damage *_sna_damage_add_rectangles(struct sna_damage *damage,
					      const xRectangle *r, int n,
					      int16_t dx, int16_t dy);
static inline void sna_damage_add_rectangles(struct sna_damage **damage,
					     const xRectangle *r, int n,
					     int16_t dx, int16_t dy)
{
	if (damage)
		*damage = _sna_damage_add_rectangles(*damage, r, n, dx, dy);
}

struct sna_damage *_sna_damage_is_all(struct sna_damage *damage,
				       int width, int height);
static inline bool sna_damage_is_all(struct sna_damage **damage,
				     int width, int height)
{
	if (*damage == NULL)
		return false;

	switch ((*damage)->mode) {
	case DAMAGE_ALL:
		return true;
	case DAMAGE_SUBTRACT:
		return false;
	default:
	case DAMAGE_ADD:
		*damage = _sna_damage_is_all(*damage, width, height);
		return (*damage)->mode == DAMAGE_ALL;
	}
}

struct sna_damage *_sna_damage_all(struct sna_damage *damage,
				   int width, int height);
static inline void sna_damage_all(struct sna_damage **damage,
				  int width, int height)
{
	*damage = _sna_damage_all(*damage, width, height);
}

fastcall struct sna_damage *_sna_damage_subtract(struct sna_damage *damage,
						 RegionPtr region);
static inline void sna_damage_subtract(struct sna_damage **damage,
				       RegionPtr region)
{
	*damage = _sna_damage_subtract(*damage, region);
}

fastcall struct sna_damage *_sna_damage_subtract_box(struct sna_damage *damage,
						     const BoxRec *box);
static inline void sna_damage_subtract_box(struct sna_damage **damage,
					   const BoxRec *box)
{
	*damage = _sna_damage_subtract_box(*damage, box);
}

Bool sna_damage_intersect(struct sna_damage *damage,
			  RegionPtr region, RegionPtr result);

int sna_damage_contains_box(struct sna_damage *damage,
			    const BoxRec *box);

int sna_damage_get_boxes(struct sna_damage *damage, BoxPtr *boxes);

struct sna_damage *_sna_damage_reduce(struct sna_damage *damage);
static inline void sna_damage_reduce(struct sna_damage **damage)
{
	if (*damage == NULL)
		return;

	*damage = _sna_damage_reduce(*damage);
}

void __sna_damage_destroy(struct sna_damage *damage);
static inline void sna_damage_destroy(struct sna_damage **damage)
{
	if (*damage == NULL)
		return;

	__sna_damage_destroy(*damage);
	*damage = NULL;
}

#if DEBUG_DAMAGE && TEST_DAMAGE
void sna_damage_selftest(void);
#else
static inline void sna_damage_selftest(void) {}
#endif

#endif /* SNA_DAMAGE_H */
