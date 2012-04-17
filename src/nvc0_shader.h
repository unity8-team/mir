#ifndef __NVC0_SHADER_H__
#define __NVC0_SHADER_H__

#define NVC0PushProgram(pNv,addr,code) do {                                    \
	const unsigned size = sizeof(code) / sizeof(code[0]);                  \
	PUSH_DATAu((pNv)->pushbuf, (pNv)->scratch, (addr), size);              \
	PUSH_DATAp((pNv)->pushbuf, (code), size);                              \
} while(0)

static uint32_t
NVC0VP_Passthrough[] = {
	0x00020461,
	0x00000000,
	0x00000000,
	0x00000000,
	0x000ff000,
	0x00000000, /* VP_ATTR_EN[0x000] */
	0x0001033f, /* VP_ATTR_EN[0x080] */
	0x00000000, /* VP_ATTR_EN[0x100] */
	0x00000000,
	0x00000000, /* VP_ATTR_EN[0x200] */
	0x00000000,
	0x00000000, /* VP_ATTR_EN[0x300] */
	0x00000000,
	0x0033f000, /* VP_EXPORT_EN[0x040] */
	0x00000000, /* VP_EXPORT_EN[0x0c0] */
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000, /* VP_EXPORT_EN[0x2c0] */
	0x00000000,
	0xfff01c66,
	0x06000080, /* vfetch { $r0,1,2,3 } b128 a[0x80] */
	0xfff11c26,
	0x06000090, /* vfetch { $r4,5 } b64 a[0x90] */
	0xfff19c26,
	0x060000a0, /* vfetch { $r6,7 } b64 a[0xa0] */
	0x03f01c66,
	0x0a7e0070, /* export v[0x70] { $r0 $r1 $r2 $r3 } */
	0x13f01c26,
	0x0a7e0080, /* export v[0x80] { $r4 $r5 } */
	0x1bf01c26,
	0x0a7e0090, /* export v[0x90] { $r6 $r7 } */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_Source[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x0000000a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f05c40,
	0xc07e0084, /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	0x03f01c40,
	0xc07e0080, /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	0xfc001e86,
	0x8013c000, /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_Composite[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x00000a0a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f0dc40,
	0xc07e0094, /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	0x03f09c40,
	0xc07e0090, /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	0xfc211e86,
	0x80120001, /* tex { _,_,_,$r4 } $t1 { $r2,3 } */
	0x03f05c40,
	0xc07e0084, /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	0x03f01c40,
	0xc07e0080, /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	0xfc001e86,
	0x8013c000, /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	0x1030dc40,
	0x58000000, /* mul ftz rn f32 $r3 $r3 $r4 */
	0x10209c40,
	0x58000000, /* mul ftz rn f32 $r2 $r2 $r4 */
	0x10105c40,
	0x58000000, /* mul ftz rn f32 $r1 $r1 $r4 */
	0x10001c40,
	0x58000000, /* mul ftz rn f32 $r0 $r0 $r4 */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_CAComposite[] = {
	0x00021462, /* 0x0000c000 = USES_KIL, MULTI_COLORS */
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000, /* FRAG_COORD_UMASK = 0x8 */
	0x00000a0a, /* FP_INTERP[0x080], 0022 0022 */
	0x00000000, /* FP_INTERP[0x0c0], 0 = OFF */
	0x00000000, /* FP_INTERP[0x100], 1 = FLAT */
	0x00000000, /* FP_INTERP[0x140], 2 = PERSPECTIVE */
	0x00000000, /* FP_INTERP[0x180], 3 = LINEAR */
	0x00000000, /* FP_INTERP[0x1c0] */
	0x00000000, /* FP_INTERP[0x200] */
	0x00000000, /* FP_INTERP[0x240] */
	0x00000000, /* FP_INTERP[0x280] */
	0x00000000, /* FP_INTERP[0x2c0] */
	0x00000000, /* FP_INTERP[0x300] */
	0x00000000,
	0x0000000f, /* FP_RESULT_MASK (0x8000 Face ?) */
	0x00000000, /* 0x2 = FragDepth, 0x1 = SampleMask */
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f0dc40,
	0xc07e0094, /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	0x03f09c40,
	0xc07e0090, /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	0xfc211e86,
	0x8013c001, /* tex { $r4,5,6,7 } $t1 { $r2,3 } */
	0x03f05c40,
	0xc07e0084, /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	0x03f01c40,
	0xc07e0080, /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	0xfc001e86,
	0x8013c000, /* tex { $r0,1,2,3 } $t0 { $r0,1 } */
	0x1c30dc40,
	0x58000000, /* mul ftz rn f32 $r3 $r3 $r7 */
	0x18209c40,
	0x58000000, /* mul ftz rn f32 $r2 $r2 $r6 */
	0x14105c40,
	0x58000000, /* mul ftz rn f32 $r1 $r1 $r5 */
	0x10001c40,
	0x58000000, /* mul ftz rn f32 $r0 $r0 $r4 */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_CACompositeSrcAlpha[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x00000a0a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f0dc40,
	0xc07e0084, /* pinterp f32 $r3 $r0 v[$r63+0x84] */
	0x03f09c40,
	0xc07e0080, /* pinterp f32 $r2 $r0 v[$r63+0x80] */
	0xfc211e86,
	0x80120000, /* tex { _,_,_,$r4 } $t0 { $r2,3 } */
	0x03f05c40,
	0xc07e0094, /* pinterp f32 $r1 $r0 v[$r63+0x94] */
	0x03f01c40,
	0xc07e0090, /* pinterp f32 $r0 $r0 v[$r63+0x90] */
	0xfc001e86,
	0x8013c001, /* tex { $r0,1,2,3 } $t1 { $r0,1 } */
	0x1030dc40,
	0x58000000, /* mul ftz rn f32 $r3 $r3 $r4 */
	0x10209c40,
	0x58000000, /* mul ftz rn f32 $r2 $r2 $r4 */
	0x10105c40,
	0x58000000, /* mul ftz rn f32 $r1 $r1 $r4 */
	0x10001c40,
	0x58000000, /* mul ftz rn f32 $r0 $r0 $r4 */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_Source_A8[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x0000000a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f05c40,
	0xc07e0084, /* pinterp f32 $r1 $r0 v[$r63+0x84] */
	0x03f01c40,
	0xc07e0080, /* pinterp f32 $r0 $r0 v[$r63+0x80] */
	0xfc001e86,
	0x80120000, /* tex { _ _ _ $r0 } $t0 { $r0 $r1 } */
	0x0000dde4,
	0x28000000, /* mov b32 $r3 $r0 */
	0x00009de4,
	0x28000000, /* mov b32 $r2 $r0 */
	0x00005de4,
	0x28000000, /* mov b32 $r1 $r0 */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_Composite_A8[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x00000a0a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff01c00,
	0xc07e007c, /* linterp f32 $r0 v[$r63+0x7c] */
	0x10001c00,
	0xc8000000, /* rcp f32 $r0 $r0 */
	0x03f0dc40,
	0xc07e0094, /* pinterp f32 $r3 $r0 v[$r63+0x94] */
	0x03f09c40,
	0xc07e0090, /* pinterp f32 $r2 $r0 v[$r63+0x90] */
	0xfc205e86,
	0x80120001, /* tex { _ _ _ $r1 } $t1 { $r2 $r3 } */
	0x03f0dc40,
	0xc07e0084, /* pinterp f32 $r3 $r0 v[$r63+0x84] */
	0x03f09c40,
	0xc07e0080, /* pinterp f32 $r2 $r0 v[$r63+0x80] */
	0xfc201e86,
	0x80120000, /* tex { _ _ _ $r0 } $t0 { $r2 $r3 } */
	0x0400dc40,
	0x58000000, /* mul ftz rn f32 $r3 $r0 $r1 */
	0x0c009de4,
	0x28000000, /* mov b32 $r2 $r3 */
	0x0c005de4,
	0x28000000, /* mov b32 $r1 $r3 */
	0x0c001de4,
	0x28000000, /* mov b32 $r0 $r3 */
	0x00001de7,
	0x80000000, /* exit */
};

static uint32_t
NVC0FP_NV12[] = {
	0x00021462,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x80000000,
	0x00000a0a,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x0000000f,
	0x00000000,
	0xfff09c00,
	0xc07e007c,
	0x10209c00,
	0xc8000000,
	0x0bf01c40,
	0xc07e0080,
	0x0bf05c40,
	0xc07e0084,
	0xfc001e86,
	0x80120000,
	0x00015c40,
	0x58004000,
	0x1050dc20,
	0x50004000,
	0x20511c20,
	0x50004000,
	0x30515c20,
	0x50004000,
	0x0bf01c40,
	0xc07e0090,
	0x0bf05c40,
	0xc07e0094,
	0xfc001e86,
	0x80130001,
	0x4000dc40,
	0x30064000,
	0x50011c40,
	0x30084000,
	0x60015c40,
	0x300a4000,
	0x70101c40,
	0x30064000,
	0x90109c40,
	0x300a4000,
	0x80105c40,
	0x30084000,
	0x00001de7,
	0x80000000,
};

#endif
