#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"

#include "intel.h"
#include "intel_transform.h"

#define xFixedToFloat(val) \
	((float)xFixedToInt(val) + ((float)xFixedFrac(val) / 65536.0))

static Bool
_intel_transform_point(PictTransformPtr transform,
		       float x, float y, float result[3])
{
	int j;

	for (j = 0; j < 3; j++) {
		result[j] = (xFixedToFloat(transform->matrix[j][0]) * x +
			     xFixedToFloat(transform->matrix[j][1]) * y +
			     xFixedToFloat(transform->matrix[j][2]));
	}
	if (!result[2])
		return FALSE;
	return TRUE;
}

/**
 * Returns the floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
intel_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				  float *x_out, float *y_out)
{
	if (transform == NULL) {
		*x_out = x;
		*y_out = y;
	} else {
		float result[3];

		if (!_intel_transform_point(transform,
					    x, y,
					    result))
			return FALSE;
		*x_out = result[0] / result[2];
		*y_out = result[1] / result[2];
	}
	return TRUE;
}

/**
 * Returns the un-normalized floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
intel_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				     float *x_out, float *y_out, float *w_out)
{
	if (transform == NULL) {
		*x_out = x;
		*y_out = y;
		*w_out = 1;
	} else {
		float result[3];

		if (!_intel_transform_point(transform,
					    x, y,
					    result))
			return FALSE;
		*x_out = result[0];
		*y_out = result[1];
		*w_out = result[2];
	}
	return TRUE;
}

/**
 * Returns whether the provided transform is affine.
 *
 * transform may be null.
 */
Bool intel_transform_is_affine(PictTransformPtr t)
{
	if (t == NULL)
		return TRUE;
	return t->matrix[2][0] == 0 && t->matrix[2][1] == 0;
}

