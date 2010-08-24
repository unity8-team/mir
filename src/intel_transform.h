#ifndef INTEL_TRANSFORM_H
#define INTEL_TRANSFORM_H

Bool
intel_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				  float *x_out, float *y_out);

Bool
intel_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				     float *x_out, float *y_out, float *w_out);

Bool intel_transform_is_affine(PictTransformPtr t);

#endif
