
INTEL_G4A = dual_prime_igd.g4a          \
        field_backward_igd.g4a          \
        field_f_b_igd.g4a               \
        field_forward_igd.g4a           \
        frame_backward_igd.g4a          \
        frame_f_b_igd.g4a               \
        frame_forward_igd.g4a           \
        dual_prime.g4a                  \
        field_backward.g4a              \
        field_f_b.g4a                   \
        field_forward.g4a               \
        frame_backward.g4a              \
        frame_f_b.g4a                   \
        frame_forward.g4a               \
		lib_igd.g4a						\
        ipicture.g4a                    \
        ipicture_igd.g4a                \
        null.g4a

INTEL_G4I = addidct.g4i                          \
        block_clear.g4i                          \
        read_frame_x0y0_y.g4i                    \
        read_frame_x0y1_y.g4i                    \
        read_frame_x1y0_y.g4i                    \
        read_frame_x1y1_y.g4i                    \
        read_frame_x0y0_uv.g4i                   \
        read_frame_x0y1_uv.g4i                   \
        read_frame_x1y0_uv.g4i                   \
        read_frame_x1y1_uv.g4i                   \
        read_frame_x0y0_y_igd.g4i                \
        read_frame_x0y1_y_igd.g4i                \
        read_frame_x1y0_y_igd.g4i                \
        read_frame_x1y1_y_igd.g4i                \
        read_frame_x0y0_uv_igd.g4i               \
        read_frame_x0y1_uv_igd.g4i               \
        read_frame_x1y0_uv_igd.g4i               \
        read_frame_x1y1_uv_igd.g4i               \
        motion_frame_y.g4i                       \
        motion_frame_uv.g4i                      \
        read_field_x0y0_y.g4i                    \
        read_field_x0y1_y.g4i                    \
        read_field_x1y0_y.g4i                    \
        read_field_x1y1_y.g4i                    \
        read_field_x0y0_uv.g4i                   \
        read_field_x0y1_uv.g4i                   \
        read_field_x1y0_uv.g4i                   \
        read_field_x1y1_uv.g4i                   \
        read_field_x0y0_y_igd.g4i                \
        read_field_x0y1_y_igd.g4i                \
        read_field_x1y0_y_igd.g4i                \
        read_field_x1y1_y_igd.g4i                \
        read_field_x0y0_uv_igd.g4i               \
        read_field_x0y1_uv_igd.g4i               \
        read_field_x1y0_uv_igd.g4i               \
        read_field_x1y1_uv_igd.g4i               \
        motion_field_y.g4i                       \
        motion_field_uv.g4i                      \
        motion_field_uv_igd.g4i                  \
        motion_field_y_igd.g4i                   \
        motion_frame_uv_igd.g4i                  \
        motion_frame_y_igd.g4i

INTEL_G4B = dual_prime.g4b          \
        field_backward.g4b          \
        field_f_b.g4b               \
        field_forward.g4b           \
        frame_backward.g4b          \
        frame_f_b.g4b               \
        frame_forward.g4b           \
        dual_prime_igd.g4b          \
        field_backward_igd.g4b      \
        field_f_b_igd.g4b           \
        field_forward_igd.g4b       \
        frame_backward_igd.g4b      \
        frame_f_b_igd.g4b           \
        frame_forward_igd.g4b       \
        ipicture.g4b                \
        ipicture_igd.g4b            \
		lib_igd.g4b					\
        null.g4b

EXTRA_DIST = $(INTEL_G4A)    \
        $(INTEL_G4I)         \
        $(INTEL_G4B)    

if HAVE_GEN4ASM

SUFFIXES = .g4a .g4b
.g4a.g4b:
	m4 $*.g4a > $*.g4m && intel-gen4asm -o $@ $*.g4m && rm $*.g4m

$(INTEL_G4B): $(INTEL_G4I)

BUILT_SOURCES= $(INTEL_G4B)

clean-local:
	-rm -f $(INTEL_G4B)
endif
