/*
 * New regs for broadwater -- we need to split this file up sensibly somehow.
 */
#define BRW_3D(Pipeline,Opcode,Subopcode) ((3 << 29) | \
					   ((Pipeline) << 27) | \
					   ((Opcode) << 24) | \
					   ((Subopcode) << 16))

#define BRW_URB_FENCE				BRW_3D(0, 0, 0)
#define BRW_CS_URB_STATE			BRW_3D(0, 0, 1)
#define BRW_CONSTANT_BUFFER			BRW_3D(0, 0, 2)
#define BRW_STATE_PREFETCH			BRW_3D(0, 0, 3)

#define BRW_STATE_BASE_ADDRESS			BRW_3D(0, 1, 1)
#define BRW_STATE_SIP				BRW_3D(0, 1, 2)
#define BRW_PIPELINE_SELECT			BRW_3D(0, 1, 4)

#define NEW_PIPELINE_SELECT			BRW_3D(1, 1, 4)

#define BRW_MEDIA_STATE_POINTERS		BRW_3D(2, 0, 0)
#define BRW_MEDIA_OBJECT			BRW_3D(2, 1, 0)

#define BRW_3DSTATE_PIPELINED_POINTERS		BRW_3D(3, 0, 0)
#define BRW_3DSTATE_BINDING_TABLE_POINTERS	BRW_3D(3, 0, 1)
# define GEN6_3DSTATE_BINDING_TABLE_MODIFY_PS  (1 << 12)/* for GEN6 */
# define GEN6_3DSTATE_BINDING_TABLE_MODIFY_GS  (1 << 9) /* for GEN6 */
# define GEN6_3DSTATE_BINDING_TABLE_MODIFY_VS  (1 << 8) /* for GEN6 */

#define BRW_3DSTATE_VERTEX_BUFFERS		BRW_3D(3, 0, 8)
#define BRW_3DSTATE_VERTEX_ELEMENTS		BRW_3D(3, 0, 9)
#define BRW_3DSTATE_INDEX_BUFFER		BRW_3D(3, 0, 0xa)
#define BRW_3DSTATE_VF_STATISTICS		BRW_3D(3, 0, 0xb)

#define BRW_3DSTATE_DRAWING_RECTANGLE		BRW_3D(3, 1, 0)
#define BRW_3DSTATE_CONSTANT_COLOR		BRW_3D(3, 1, 1)
#define BRW_3DSTATE_SAMPLER_PALETTE_LOAD	BRW_3D(3, 1, 2)
#define BRW_3DSTATE_CHROMA_KEY			BRW_3D(3, 1, 4)
#define BRW_3DSTATE_DEPTH_BUFFER		BRW_3D(3, 1, 5)
# define BRW_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT	29
# define BRW_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT	18

#define BRW_3DSTATE_POLY_STIPPLE_OFFSET		BRW_3D(3, 1, 6)
#define BRW_3DSTATE_POLY_STIPPLE_PATTERN	BRW_3D(3, 1, 7)
#define BRW_3DSTATE_LINE_STIPPLE		BRW_3D(3, 1, 8)
#define BRW_3DSTATE_GLOBAL_DEPTH_OFFSET_CLAMP	BRW_3D(3, 1, 9)
/* These two are BLC and CTG only, not BW or CL */
#define BRW_3DSTATE_AA_LINE_PARAMS		BRW_3D(3, 1, 0xa)
#define BRW_3DSTATE_GS_SVB_INDEX		BRW_3D(3, 1, 0xb)

#define BRW_PIPE_CONTROL			BRW_3D(3, 2, 0)

#define BRW_3DPRIMITIVE				BRW_3D(3, 3, 0)

#define BRW_3DSTATE_CLEAR_PARAMS		BRW_3D(3, 1, 0x10)
/* DW1 */
# define BRW_3DSTATE_DEPTH_CLEAR_VALID		(1 << 15)

/* for GEN6+ */
#define GEN6_3DSTATE_SAMPLER_STATE_POINTERS	BRW_3D(3, 0, 0x02)
# define GEN6_3DSTATE_SAMPLER_STATE_MODIFY_PS	(1 << 12)
# define GEN6_3DSTATE_SAMPLER_STATE_MODIFY_GS	(1 << 9)
# define GEN6_3DSTATE_SAMPLER_STATE_MODIFY_VS	(1 << 8)

#define GEN6_3DSTATE_URB			BRW_3D(3, 0, 0x05)
/* DW1 */
# define GEN6_3DSTATE_URB_VS_SIZE_SHIFT		16
# define GEN6_3DSTATE_URB_VS_ENTRIES_SHIFT	0
/* DW2 */
# define GEN6_3DSTATE_URB_GS_ENTRIES_SHIFT	8
# define GEN6_3DSTATE_URB_GS_SIZE_SHIFT		0

#define GEN6_3DSTATE_VIEWPORT_STATE_POINTERS	BRW_3D(3, 0, 0x0d)
# define GEN6_3DSTATE_VIEWPORT_STATE_MODIFY_CC		(1 << 12)
# define GEN6_3DSTATE_VIEWPORT_STATE_MODIFY_SF		(1 << 11)
# define GEN6_3DSTATE_VIEWPORT_STATE_MODIFY_CLIP	(1 << 10)

#define GEN6_3DSTATE_CC_STATE_POINTERS		BRW_3D(3, 0, 0x0e)

#define GEN6_3DSTATE_VS				BRW_3D(3, 0, 0x10)

#define GEN6_3DSTATE_GS				BRW_3D(3, 0, 0x11)
/* DW4 */
# define GEN6_3DSTATE_GS_DISPATCH_START_GRF_SHIFT	0

#define GEN6_3DSTATE_CLIP			BRW_3D(3, 0, 0x12)

#define GEN6_3DSTATE_SF				BRW_3D(3, 0, 0x13)
/* DW1 */
# define GEN6_3DSTATE_SF_NUM_OUTPUTS_SHIFT		22
# define GEN6_3DSTATE_SF_URB_ENTRY_READ_LENGTH_SHIFT	11
# define GEN6_3DSTATE_SF_URB_ENTRY_READ_OFFSET_SHIFT	4
/* DW2 */
/* DW3 */
# define GEN6_3DSTATE_SF_CULL_BOTH			(0 << 29)
# define GEN6_3DSTATE_SF_CULL_NONE			(1 << 29)
# define GEN6_3DSTATE_SF_CULL_FRONT			(2 << 29)
# define GEN6_3DSTATE_SF_CULL_BACK			(3 << 29)
/* DW4 */
# define GEN6_3DSTATE_SF_TRI_PROVOKE_SHIFT		29
# define GEN6_3DSTATE_SF_LINE_PROVOKE_SHIFT		27
# define GEN6_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT		25


#define GEN6_3DSTATE_WM				BRW_3D(3, 0, 0x14)
/* DW2 */
# define GEN6_3DSTATE_WM_SAMPLER_COUNT_SHITF			27
# define GEN6_3DSTATE_WM_BINDING_TABLE_ENTRY_COUNT_SHIFT	18
/* DW4 */
# define GEN6_3DSTATE_WM_DISPATCH_START_GRF_0_SHIFT		16
/* DW5 */
# define GEN6_3DSTATE_WM_MAX_THREADS_SHIFT			25
# define GEN6_3DSTATE_WM_DISPATCH_ENABLE			(1 << 19)
# define GEN6_3DSTATE_WM_16_DISPATCH_ENABLE			(1 << 1)
# define GEN6_3DSTATE_WM_8_DISPATCH_ENABLE			(1 << 0)
/* DW6 */
# define GEN6_3DSTATE_WM_NUM_SF_OUTPUTS_SHIFT			20
# define GEN6_3DSTATE_WM_NONPERSPECTIVE_SAMPLE_BARYCENTRIC	(1 << 15)
# define GEN6_3DSTATE_WM_NONPERSPECTIVE_CENTROID_BARYCENTRIC	(1 << 14)
# define GEN6_3DSTATE_WM_NONPERSPECTIVE_PIXEL_BARYCENTRIC	(1 << 13)
# define GEN6_3DSTATE_WM_PERSPECTIVE_SAMPLE_BARYCENTRIC		(1 << 12)
# define GEN6_3DSTATE_WM_PERSPECTIVE_CENTROID_BARYCENTRIC	(1 << 11)
# define GEN6_3DSTATE_WM_PERSPECTIVE_PIXEL_BARYCENTRIC		(1 << 10)


#define GEN6_3DSTATE_CONSTANT_VS		BRW_3D(3, 0, 0x15)
#define GEN6_3DSTATE_CONSTANT_GS          	BRW_3D(3, 0, 0x16)
#define GEN6_3DSTATE_CONSTANT_PS          	BRW_3D(3, 0, 0x17)

#define GEN6_3DSTATE_SAMPLE_MASK		BRW_3D(3, 0, 0x18)

#define GEN6_3DSTATE_MULTISAMPLE		BRW_3D(3, 1, 0x0d)
/* DW1 */
# define GEN6_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER		(0 << 4)
# define GEN6_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_UPPER_LEFT	(1 << 4)
# define GEN6_3DSTATE_MULTISAMPLE_NUMSAMPLES_1			(0 << 1)
# define GEN6_3DSTATE_MULTISAMPLE_NUMSAMPLES_4			(2 << 1)
# define GEN6_3DSTATE_MULTISAMPLE_NUMSAMPLES_8			(3 << 1)

#define PIPELINE_SELECT_3D		0
#define PIPELINE_SELECT_MEDIA		1

#define UF0_CS_REALLOC			(1 << 13)
#define UF0_VFE_REALLOC			(1 << 12)
#define UF0_SF_REALLOC			(1 << 11)
#define UF0_CLIP_REALLOC		(1 << 10)
#define UF0_GS_REALLOC			(1 << 9)
#define UF0_VS_REALLOC			(1 << 8)
#define UF1_CLIP_FENCE_SHIFT		20
#define UF1_GS_FENCE_SHIFT		10
#define UF1_VS_FENCE_SHIFT		0
#define UF2_CS_FENCE_SHIFT		20
#define UF2_VFE_FENCE_SHIFT		10
#define UF2_SF_FENCE_SHIFT		0

/* for BRW_STATE_BASE_ADDRESS */
#define BASE_ADDRESS_MODIFY		(1 << 0)

/* for BRW_3DSTATE_PIPELINED_POINTERS */
#define BRW_GS_DISABLE		       0
#define BRW_GS_ENABLE		       1
#define BRW_CLIP_DISABLE	       0
#define BRW_CLIP_ENABLE		       1

/* for BRW_PIPE_CONTROL */
#define BRW_PIPE_CONTROL_NOWRITE       (0 << 14)
#define BRW_PIPE_CONTROL_WRITE_QWORD   (1 << 14)
#define BRW_PIPE_CONTROL_WRITE_DEPTH   (2 << 14)
#define BRW_PIPE_CONTROL_WRITE_TIME    (3 << 14)
#define BRW_PIPE_CONTROL_DEPTH_STALL   (1 << 13)
#define BRW_PIPE_CONTROL_WC_FLUSH      (1 << 12)
#define BRW_PIPE_CONTROL_IS_FLUSH      (1 << 11)
#define BRW_PIPE_CONTROL_NOTIFY_ENABLE (1 << 8)
#define BRW_PIPE_CONTROL_GLOBAL_GTT    (1 << 2)
#define BRW_PIPE_CONTROL_LOCAL_PGTT    (0 << 2)
#define BRW_PIPE_CONTROL_DEPTH_CACHE_FLUSH	(1 << 0)

/* VERTEX_BUFFER_STATE Structure */
#define VB0_BUFFER_INDEX_SHIFT		27
#define GEN6_VB0_BUFFER_INDEX_SHIFT	26
#define VB0_VERTEXDATA			(0 << 26)
#define VB0_INSTANCEDATA		(1 << 26)
#define GEN6_VB0_VERTEXDATA		(0 << 20)
#define GEN6_VB0_INSTANCEDATA		(1 << 20)
#define VB0_BUFFER_PITCH_SHIFT		0

/* VERTEX_ELEMENT_STATE Structure */
#define VE0_VERTEX_BUFFER_INDEX_SHIFT	27
#define GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT	26 /* for GEN6 */
#define VE0_VALID			(1 << 26)
#define GEN6_VE0_VALID			(1 << 25) /* for GEN6 */
#define VE0_FORMAT_SHIFT		16
#define VE0_OFFSET_SHIFT		0
#define VE1_VFCOMPONENT_0_SHIFT		28
#define VE1_VFCOMPONENT_1_SHIFT		24
#define VE1_VFCOMPONENT_2_SHIFT		20
#define VE1_VFCOMPONENT_3_SHIFT		16
#define VE1_DESTINATION_ELEMENT_OFFSET_SHIFT	0

/* 3DPRIMITIVE bits */
#define BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
#define BRW_3DPRIMITIVE_VERTEX_RANDOM	  (1 << 15)
/* Primitive types are in brw_defines.h */
#define BRW_3DPRIMITIVE_TOPOLOGY_SHIFT	  10

#define BRW_SVG_CTL		       0x7400

#define BRW_SVG_CTL_GS_BA	       (0 << 8)
#define BRW_SVG_CTL_SS_BA	       (1 << 8)
#define BRW_SVG_CTL_IO_BA	       (2 << 8)
#define BRW_SVG_CTL_GS_AUB	       (3 << 8)
#define BRW_SVG_CTL_IO_AUB	       (4 << 8)
#define BRW_SVG_CTL_SIP		       (5 << 8)

#define BRW_SVG_RDATA		       0x7404
#define BRW_SVG_WORK_CTL	       0x7408

#define BRW_VF_CTL		       0x7500

#define BRW_VF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_THREADID	   (0 << 8)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_VF_DEBUG	   (1 << 8)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_SEQUENCE   (0 << 4)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_INDEX	   (1 << 4)
#define BRW_VF_CTL_SKIP_INITIAL_PRIMITIVES	   (1 << 3)
#define BRW_VF_CTL_MAX_PRIMITIVES_LIMIT_ENABLE	   (1 << 2)
#define BRW_VF_CTL_VERTEX_RANGE_LIMIT_ENABLE	   (1 << 1)
#define BRW_VF_CTL_SNAPSHOT_ENABLE	     	   (1 << 0)

#define BRW_VF_STRG_VAL		       0x7504
#define BRW_VF_STR_VL_OVR	       0x7508
#define BRW_VF_VC_OVR		       0x750c
#define BRW_VF_STR_PSKIP	       0x7510
#define BRW_VF_MAX_PRIM		       0x7514
#define BRW_VF_RDATA		       0x7518

#define BRW_VS_CTL		       0x7600
#define BRW_VS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_0	   (0 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_1	   (1 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VALID_COUNT	   (2 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VS_KERNEL_POINTER  (3 << 8)
#define BRW_VS_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_VS_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_VS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_VS_STRG_VAL		       0x7604
#define BRW_VS_RDATA		       0x7608

#define BRW_SF_CTL		       0x7b00
#define BRW_SF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_FF_ID	   (0 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_REL_COUNT (1 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_FF_ID	   (2 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_REL_COUNT (3 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_FF_ID	   (4 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_REL_COUNT (5 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_COUNT	   (6 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_SF_KERNEL_POINTER  (7 << 8)
#define BRW_SF_CTL_MIN_MAX_PRIMITIVE_RANGE_ENABLE  (1 << 4)
#define BRW_SF_CTL_DEBUG_CLIP_RECTANGLE_ENABLE	   (1 << 3)
#define BRW_SF_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_SF_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_SF_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_SF_STRG_VAL		       0x7b04
#define BRW_SF_RDATA		       0x7b18

#define BRW_WIZ_CTL		       0x7c00
#define BRW_WIZ_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_WIZ_CTL_SUBSPAN_INSTANCE_SHIFT	   16
#define BRW_WIZ_CTL_SNAPSHOT_MUX_WIZ_KERNEL_POINTER   (0 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_SUBSPAN_INSTANCE     (1 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_PRIMITIVE_SEQUENCE   (2 << 8)
#define BRW_WIZ_CTL_SINGLE_SUBSPAN_DISPATCH	      (1 << 6)
#define BRW_WIZ_CTL_IGNORE_COLOR_SCOREBOARD_STALLS    (1 << 5)
#define BRW_WIZ_CTL_ENABLE_SUBSPAN_INSTANCE_COMPARE   (1 << 4)
#define BRW_WIZ_CTL_USE_UPSTREAM_SNAPSHOT_FLAG	      (1 << 3)
#define BRW_WIZ_CTL_SNAPSHOT_ALL_THREADS	      (1 << 2)
#define BRW_WIZ_CTL_THREAD_SNAPSHOT_ENABLE	      (1 << 1)
#define BRW_WIZ_CTL_SNAPSHOT_ENABLE		      (1 << 0)

#define BRW_WIZ_STRG_VAL			      0x7c04
#define BRW_WIZ_RDATA				      0x7c18

#define BRW_TS_CTL		       0x7e00
#define BRW_TS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_TS_CTL_SNAPSHOT_MESSAGE_ERROR	   (0 << 8)
#define BRW_TS_CTL_SNAPSHOT_INTERFACE_DESCRIPTOR   (3 << 8)
#define BRW_TS_CTL_SNAPSHOT_ALL_CHILD_THREADS	   (1 << 2)
#define BRW_TS_CTL_SNAPSHOT_ALL_ROOT_THREADS  	   (1 << 1)
#define BRW_TS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_TS_STRG_VAL		       0x7e04
#define BRW_TS_RDATA		       0x7e08

#define BRW_TD_CTL		       0x8000
#define BRW_TD_CTL_MUX_SHIFT	       8
#define BRW_TD_CTL_EXTERNAL_HALT_R0_DEBUG_MATCH	   (1 << 7)
#define BRW_TD_CTL_FORCE_EXTERNAL_HALT		   (1 << 6)
#define BRW_TD_CTL_EXCEPTION_MASK_OVERRIDE	   (1 << 5)
#define BRW_TD_CTL_FORCE_THREAD_BREAKPOINT_ENABLE  (1 << 4)
#define BRW_TD_CTL_BREAKPOINT_ENABLE		   (1 << 2)
#define BRW_TD_CTL2		       0x8004
#define BRW_TD_CTL2_ILLEGAL_OPCODE_EXCEPTION_OVERRIDE (1 << 28)
#define BRW_TD_CTL2_MASKSTACK_EXCEPTION_OVERRIDE      (1 << 26)
#define BRW_TD_CTL2_SOFTWARE_EXCEPTION_OVERRIDE	      (1 << 25)
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_SHIFT	      16
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_ENABLE	      (1 << 8)
#define BRW_TD_CTL2_THREAD_SPAWNER_EXECUTION_MASK_ENABLE (1 << 7)
#define BRW_TD_CTL2_WIZ_EXECUTION_MASK_ENABLE	      (1 << 6)
#define BRW_TD_CTL2_SF_EXECUTION_MASK_ENABLE	      (1 << 5)
#define BRW_TD_CTL2_CLIPPER_EXECUTION_MASK_ENABLE     (1 << 4)
#define BRW_TD_CTL2_GS_EXECUTION_MASK_ENABLE	      (1 << 3)
#define BRW_TD_CTL2_VS_EXECUTION_MASK_ENABLE	      (1 << 0)
#define BRW_TD_VF_VS_EMSK	       0x8008
#define BRW_TD_GS_EMSK		       0x800c
#define BRW_TD_CLIP_EMSK	       0x8010
#define BRW_TD_SF_EMSK		       0x8014
#define BRW_TD_WIZ_EMSK		       0x8018
#define BRW_TD_0_6_EHTRG_VAL	       0x801c
#define BRW_TD_0_7_EHTRG_VAL	       0x8020
#define BRW_TD_0_6_EHTRG_MSK           0x8024
#define BRW_TD_0_7_EHTRG_MSK	       0x8028
#define BRW_TD_RDATA		       0x802c
#define BRW_TD_TS_EMSK		       0x8030

#define BRW_EU_CTL		       0x8800
#define BRW_EU_CTL_SELECT_SHIFT	       16
#define BRW_EU_CTL_DATA_MUX_SHIFT      8
#define BRW_EU_ATT_0		       0x8810
#define BRW_EU_ATT_1		       0x8814
#define BRW_EU_ATT_DATA_0	       0x8820
#define BRW_EU_ATT_DATA_1	       0x8824
#define BRW_EU_ATT_CLR_0	       0x8830
#define BRW_EU_ATT_CLR_1	       0x8834
#define BRW_EU_RDATA		       0x8840

/* End regs for broadwater */

