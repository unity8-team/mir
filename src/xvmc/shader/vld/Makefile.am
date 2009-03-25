INTEL_G4A = ipicture.g4a 	\
	    lib.g4a		\
	    frame_forward.g4a	\
	    frame_backward.g4a	\
	    frame_f_b.g4a	\
	    field_forward.g4a	\
	    field_backward.g4a	\
	    field_f_b.g4a	

INTEL_G4B = ipicture.g4b	\
	    lib.g4b		\
	    frame_forward.g4b	\
	    frame_backward.g4b	\
	    frame_f_b.g4b	\
	    field_forward.g4b	\
	    field_backward.g4b  \
	    field_f_b.g4b

if HAVE_GEN4ASM

SUFFIXES = .g4a .g4b
.g4a.g4b:
	m4 $*.g4a > $*.g4m && intel-gen4asm -o $@ $*.g4m && rm $*.g4m

$(INTEL_G4B): $(INTEL_G4I)

BUILT_SOURCES= $(INTEL_G4B)

clean-local:
	-rm -f $(INTEL_G4B)
endif    
