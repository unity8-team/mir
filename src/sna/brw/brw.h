#include "brw_eu.h"

void brw_sf_kernel__nomask(struct brw_compile *p);
void brw_sf_kernel__mask(struct brw_compile *p);

void brw_wm_kernel__affine(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__affine_mask(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__affine_mask_ca(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__affine_mask_sa(struct brw_compile *p, int dispatch_width);

void brw_wm_kernel__projective(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__projective_mask(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__projective_mask_ca(struct brw_compile *p, int dispatch_width);
void brw_wm_kernel__projective_mask_sa(struct brw_compile *p, int dispatch_width);
