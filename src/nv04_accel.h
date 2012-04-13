#ifndef __NV04_ACCEL_H__
#define __NV04_ACCEL_H__

/* subchannel assignments */
#define SUBC_M2MF(mthd)  0, (mthd)
#define NV03_M2MF(mthd)  SUBC_M2MF(NV03_M2MF_##mthd)
#define SUBC_NVSW(mthd)  1, (mthd)
#define SUBC_SF2D(mthd)  2, (mthd)
#define NV04_SF2D(mthd)  SUBC_SF2D(NV04_SURFACE_2D_##mthd)
#define NV10_SF2D(mthd)  SUBC_SF2D(NV10_SURFACE_2D_##mthd)
#define SUBC_RECT(mthd)  3, (mthd)
#define NV04_RECT(mthd)  SUBC_RECT(NV04_GDI_##mthd)
#define SUBC_BLIT(mthd)  4, (mthd)
#define NV01_BLIT(mthd)  SUBC_BLIT(NV01_BLIT_##mthd)
#define NV04_BLIT(mthd)  SUBC_BLIT(NV04_BLIT_##mthd)
#define NV15_BLIT(mthd)  SUBC_BLIT(NV15_BLIT_##mthd)
#define SUBC_IFC(mthd)   5, (mthd)
#define NV01_IFC(mthd)   SUBC_IFC(NV01_IFC_##mthd)
#define NV04_IFC(mthd)   SUBC_IFC(NV04_IFC_##mthd)
#define SUBC_MISC(mthd)  6, (mthd)
#define NV03_SIFM(mthd)  SUBC_MISC(NV03_SIFM_##mthd)
#define NV05_SIFM(mthd)  SUBC_MISC(NV05_SIFM_##mthd)
#define NV01_BETA(mthd)  SUBC_MISC(NV01_BETA_##mthd)
#define NV04_BETA4(mthd) SUBC_MISC(NV04_BETA4_##mthd)
#define NV01_PATT(mthd)  SUBC_MISC(NV01_PATTERN_##mthd)
#define NV04_PATT(mthd)  SUBC_MISC(NV04_PATTERN_##mthd)
#define NV01_ROP(mthd)   SUBC_MISC(NV01_ROP_##mthd)
#define NV01_CLIP(mthd)  SUBC_MISC(NV01_CLIP_##mthd)
#define SUBC_3D(mthd)    7, (mthd)
#define NV10_3D(mthd)    SUBC_3D(NV10_3D_##mthd)
#define NV30_3D(mthd)    SUBC_3D(NV30_3D_##mthd)
#define NV40_3D(mthd)    SUBC_3D(NV40_3D_##mthd)

#endif
