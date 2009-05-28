/*
 * Copyright 2003 NVIDIA, Corporation
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
 * Copyright 2007-2009 Stuart Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <X11/Xatom.h>
#include <X11/Xos.h>	/* X_GETTIMEOFDAY */
#include "nv_include.h"

#define MULTIPLE_ENCODERS(e) (e & (e - 1))
#define FOR_EACH_ENCODER_IN_CONNECTOR(i, c, e)	for (i = 0; i < pNv->vbios->dcb->entries; i++)	\
							if (c->possible_encoders & (1 << i) &&	\
							    (e = &pNv->encoders[i]))

static int nv_output_ramdac_offset(struct nouveau_encoder *nv_encoder)
{
	int offset = 0;

	if (nv_encoder->dcb->or & (8 | OUTPUT_C))
		offset += 0x68;
	if (nv_encoder->dcb->or & (8 | OUTPUT_B))
		offset += 0x2000;

	return offset;
}

static int nv_get_digital_bound_head(NVPtr pNv, int or)
{
	/* special case of nv_read_tmds to find crtc associated with an output.
	 * this does not give a correct answer for off-chip dvi, but there's no
	 * use for such an answer anyway
	 */
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL,
	NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE | 0x4);
	return (((NVReadRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA) & 0x8) >> 3) ^ ramdac);
}

#define WAIT_FOR(cond, timeout_us) __extension__ ({	\
	struct timeval begin, cur;			\
	long d_secs, d_usecs, diff = 0;			\
							\
	X_GETTIMEOFDAY(&begin);				\
	while (!(cond) && diff < timeout_us) {		\
		X_GETTIMEOFDAY(&cur);			\
		d_secs  = cur.tv_sec - begin.tv_sec;	\
		d_usecs = cur.tv_usec - begin.tv_usec;	\
		diff = d_secs * 1000000 + d_usecs;	\
	};						\
	diff >= timeout_us ? -EAGAIN : 0;		\
})

/*
 * arbitrary limit to number of sense oscillations tolerated in one sample
 * period (observed to be at least 13 in "nvidia")
 */
#define MAX_HBLANK_OSC 20

/*
 * arbitrary limit to number of conflicting sample pairs to tolerate at a
 * voltage step (observed to be at least 5 in "nvidia")
 */
#define MAX_SAMPLE_PAIRS 10

static int sample_load_twice(NVPtr pNv, bool sense[2])
{
	int i;

	for (i = 0; i < 2; i++) {
		bool sense_a, sense_b, sense_b_prime;
		int j = 0;

		/*
		 * wait for bit 0 clear -- out of hblank -- (say reg value 0x4),
		 * then wait for transition 0x4->0x5->0x4: enter hblank, leave
		 * hblank again
		 * use a 10ms timeout (guards against crtc being inactive, in
		 * which case blank state would never change)
		 */
		if (WAIT_FOR(!(VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR) & 1), 10000))
			return -EWOULDBLOCK;
		if (WAIT_FOR(VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR) & 1, 10000))
			return -EWOULDBLOCK;
		if (WAIT_FOR(!(VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR) & 1), 10000))
			return -EWOULDBLOCK;

		WAIT_FOR(0, 100);	/* faster than usleep(100) */
		/* when level triggers, sense is _LO_ */
		sense_a = VGA_RD08(pNv->REGS, NV_PRMCIO_INP0) & 0x10;

		/* take another reading until it agrees with sense_a... */
		do {
			WAIT_FOR(0, 100);
			sense_b = VGA_RD08(pNv->REGS, NV_PRMCIO_INP0) & 0x10;
			if (sense_a != sense_b) {
				sense_b_prime = VGA_RD08(pNv->REGS, NV_PRMCIO_INP0) & 0x10;
				if (sense_b == sense_b_prime) {
					/* ... unless two consecutive subsequent
					 * samples agree; sense_a is replaced */
					sense_a = sense_b;
					/* force mis-match so we loop */
					sense_b = !sense_a;
				}
			}
		} while ((sense_a != sense_b) && ++j < MAX_HBLANK_OSC);

		if (j == MAX_HBLANK_OSC)
			/* with so much oscillation, default to sense:LO */
			sense[i] = false;
		else
			sense[i] = sense_a;
	}

	return 0;
}

static bool nv_legacy_load_detect(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint8_t saved_seq1, saved_pi, saved_rpc1;
	uint8_t saved_palette0[3], saved_palette_mask;
	uint32_t saved_rtest_ctrl, saved_rgen_ctrl;
	int i;
	uint8_t blue;
	bool sense = true;

	/*
	 * for this detection to work, there needs to be a mode set up on the
	 * CRTC.  this is presumed to be the case
	 */

	if (pNv->twoHeads)
		/* only implemented for head A for now */
		NVSetOwner(pNv, 0);

	saved_seq1 = NVReadVgaSeq(pNv, 0, NV_VIO_SR_CLOCK_INDEX);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_CLOCK_INDEX, saved_seq1 & ~0x20);

	saved_rtest_ctrl = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL,
		      saved_rtest_ctrl & ~NV_PRAMDAC_TEST_CONTROL_PWRDWN_DAC_OFF);

	usleep(10000);

	saved_pi = NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_PIXEL_INDEX);
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_PIXEL_INDEX,
		       saved_pi & ~(0x80 | MASK(NV_CIO_CRE_PIXEL_FORMAT)));
	saved_rpc1 = NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_RPC1_INDEX);
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_RPC1_INDEX, saved_rpc1 & ~0xc0);

	VGA_WR08(pNv->REGS, NV_PRMDIO_READ_MODE_ADDRESS, 0x0);
	for (i = 0; i < 3; i++)
		saved_palette0[i] = NV_RD08(pNv->REGS, NV_PRMDIO_PALETTE_DATA);
	saved_palette_mask = NV_RD08(pNv->REGS, NV_PRMDIO_PIXEL_MASK);
	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK, 0);

	saved_rgen_ctrl = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_GENERAL_CONTROL);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_GENERAL_CONTROL,
		      (saved_rgen_ctrl & ~(NV_PRAMDAC_GENERAL_CONTROL_BPC_8BITS |
					   NV_PRAMDAC_GENERAL_CONTROL_TERMINATION_75OHM)) |
		      NV_PRAMDAC_GENERAL_CONTROL_PIXMIX_ON);

	blue = 8;	/* start of test range */

	do {
		bool sense_pair[2];

		VGA_WR08(pNv->REGS, NV_PRMDIO_WRITE_MODE_ADDRESS, 0);
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA, 0);
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA, 0);
		/* testing blue won't find monochrome monitors.  I don't care */
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA, blue);

		i = 0;
		/* take sample pairs until both samples in the pair agree */
		do {
			if (sample_load_twice(pNv, sense_pair))
				goto out;
		} while ((sense_pair[0] != sense_pair[1]) &&
							++i < MAX_SAMPLE_PAIRS);

		if (i == MAX_SAMPLE_PAIRS)
			/* too much oscillation defaults to LO */
			sense = false;
		else
			sense = sense_pair[0];

	/*
	 * if sense goes LO before blue ramps to 0x18, monitor is not connected.
	 * ergo, if blue gets to 0x18, monitor must be connected
	 */
	} while (++blue < 0x18 && sense);

out:
	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK, saved_palette_mask);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_GENERAL_CONTROL, saved_rgen_ctrl);
	VGA_WR08(pNv->REGS, NV_PRMDIO_WRITE_MODE_ADDRESS, 0);
	for (i = 0; i < 3; i++)
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA, saved_palette0[i]);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL, saved_rtest_ctrl);
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_PIXEL_INDEX, saved_pi);
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_RPC1_INDEX, saved_rpc1);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_CLOCK_INDEX, saved_seq1);

	if (blue == 0x18) {
		NV_TRACE(pScrn, "Load detected on head A\n");
		return true;
	}

	return false;
}

static bool
nv_nv17_load_detect(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t testval, regoffset = nv_output_ramdac_offset(nv_encoder);
	uint32_t saved_powerctrl_2 = 0, saved_powerctrl_4 = 0, saved_routput, saved_rtest_ctrl, temp;
	int head, present = 0;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

#define RGB_TEST_DATA(r,g,b) (r << 0 | g << 10 | b << 20)
	testval = RGB_TEST_DATA(0x140, 0x140, 0x140); /* 0x94050140 */
	if (pNv->vbios->dactestval)
		testval = pNv->vbios->dactestval;

	saved_rtest_ctrl = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + regoffset);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + regoffset,
		      saved_rtest_ctrl & ~NV_PRAMDAC_TEST_CONTROL_PWRDWN_DAC_OFF);

	saved_powerctrl_2 = nvReadMC(pNv, NV_PBUS_POWERCTRL_2);

	nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, saved_powerctrl_2 & 0xd7ffffff);
	if (regoffset == 0x68) {
		saved_powerctrl_4 = nvReadMC(pNv, NV_PBUS_POWERCTRL_4);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_4, saved_powerctrl_4 & 0xffffffcf);
	}

	usleep(4000);

	saved_routput = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + regoffset);
	head = (saved_routput & 0x100) >> 8;
	/* if there's a spare crtc, using it will minimise flicker for the case
	 * where the in-use crtc is in use by an off-chip tmds encoder */
	if (xf86_config->crtc[head]->enabled && !xf86_config->crtc[head ^ 1]->enabled)
		head ^= 1;
	/* nv driver and nv31 use 0xfffffeee, nv34 and 6600 use 0xfffffece */
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + regoffset,
		      (saved_routput & 0xfffffece) | head << 8);
	usleep(1000);

	temp = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + regoffset);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + regoffset, temp | 1);

	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_TESTPOINT_DATA,
		      NV_PRAMDAC_TESTPOINT_DATA_NOTBLANK | testval);
	temp = NVReadRAMDAC(pNv, head, NV_PRAMDAC_TEST_CONTROL);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_TEST_CONTROL,
		      temp | NV_PRAMDAC_TEST_CONTROL_TP_INS_EN_ASSERTED);
	usleep(1000);

	present = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + regoffset) &
			NV_PRAMDAC_TEST_CONTROL_SENSEB_ALLHI;

	temp = NVReadRAMDAC(pNv, head, NV_PRAMDAC_TEST_CONTROL);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_TEST_CONTROL,
		      temp & ~NV_PRAMDAC_TEST_CONTROL_TP_INS_EN_ASSERTED);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_TESTPOINT_DATA, 0);

	/* bios does something more complex for restoring, but I think this is good enough */
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + regoffset, saved_routput);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + regoffset, saved_rtest_ctrl);
	if (regoffset == 0x68)
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_4, saved_powerctrl_4);
	nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, saved_powerctrl_2);

	if (present) {
		NV_TRACE(pScrn, "Load detected on output %c\n",
			 '@' + ffs(nv_encoder->dcb->or));
		return true;
	}

	return false;
}

static void
update_output_fields(xf86OutputPtr output, struct nouveau_encoder *det_encoder)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	NVPtr pNv = NVPTR(output->scrn);

	if (nv_connector->detected_encoder == det_encoder)
		return;

	nv_connector->detected_encoder = det_encoder;
	output->possible_crtcs = det_encoder->dcb->heads;
	if (det_encoder->dcb->type == OUTPUT_LVDS || det_encoder->dcb->type == OUTPUT_TMDS) {
		output->doubleScanAllowed = false;
		output->interlaceAllowed = false;
	} else {
		output->doubleScanAllowed = true;
		if (pNv->Architecture == NV_ARCH_20 ||
		   (pNv->Architecture == NV_ARCH_10 &&
		    (pNv->Chipset & 0x0ff0) != CHIPSET_NV10 &&
		    (pNv->Chipset & 0x0ff0) != CHIPSET_NV15))
			/* HW is broken */
			output->interlaceAllowed = false;
		else
			output->interlaceAllowed = true;
	}
}

static bool edid_sink_connected(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	NVPtr pNv = NVPTR(output->scrn);
	bool waslocked = NVLockVgaCrtcs(pNv, false);
	bool wastied = nv_heads_tied(pNv);

	if (wastied)
		NVSetOwner(pNv, 0);	/* necessary? */

	nv_connector->edid = xf86OutputGetEDID(output, nv_connector->pDDCBus);

	if (wastied)
		NVSetOwner(pNv, 0x4);
	if (waslocked)
		NVLockVgaCrtcs(pNv, true);

	return !!nv_connector->edid;
}

static xf86OutputStatus
nv_output_detect(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_encoder *det_encoder;
	xf86OutputStatus ret = XF86OutputStatusDisconnected;

	struct nouveau_encoder *find_encoder_by_type(enum nouveau_encoder_type type)
	{
		int i;
		for (i = 0; i < pNv->vbios->dcb->entries; i++)
			if (nv_connector->possible_encoders & (1 << i) &&
			    (type == OUTPUT_ANY || pNv->encoders[i].dcb->type == type))
				return &pNv->encoders[i];
		return NULL;
	}

	/* if an LVDS output was ever connected it remains so */
	if (nv_connector->detected_encoder &&
	    nv_connector->detected_encoder->dcb->type == OUTPUT_LVDS)
		return XF86OutputStatusConnected;

	if (nv_connector->pDDCBus && edid_sink_connected(output)) {
		if (MULTIPLE_ENCODERS(nv_connector->possible_encoders)) {
			if (nv_connector->edid->features.input_type)
				det_encoder = find_encoder_by_type(OUTPUT_TMDS);
			else
				det_encoder = find_encoder_by_type(OUTPUT_ANALOG);
		} else
			det_encoder = find_encoder_by_type(OUTPUT_ANY);
		ret = XF86OutputStatusConnected;
	} else if ((det_encoder = find_encoder_by_type(OUTPUT_ANALOG))) {
		/* bind encoder if enabled in xorg.conf */
		if (output->conf_monitor &&
		    xf86CheckBoolOption(output->conf_monitor->mon_option_lst,
					"Enable", FALSE))
			ret = XF86OutputStatusConnected;
		else if (pNv->gf4_disp_arch) {
			if (nv_nv17_load_detect(pScrn, det_encoder))
				ret = XF86OutputStatusConnected;
		} else
			 if (nv_legacy_load_detect(pScrn))
				ret = XF86OutputStatusConnected;
	} else if ((det_encoder = find_encoder_by_type(OUTPUT_LVDS))) {
		if (det_encoder->dcb->lvdsconf.use_straps_for_mode) {
			if (nouveau_bios_fp_mode(pScrn, NULL))
				ret = XF86OutputStatusConnected;
		} else if (!pNv->vbios->fp_no_ddc &&
			   nouveau_bios_embedded_edid(pScrn)) {
			nv_connector->edid = xf86InterpretEDID(pScrn->scrnIndex,
							       nouveau_bios_embedded_edid(pScrn));
			ret = XF86OutputStatusConnected;
		}
	}

	if (ret != XF86OutputStatusDisconnected)
		update_output_fields(output, det_encoder);

	return ret;
}

static DisplayModePtr
get_native_mode_from_edid(xf86OutputPtr output, DisplayModePtr edid_modes)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	int max_h_active = 0, max_v_active = 0;
	int i;
	DisplayModePtr mode;

	for (i = 0; i < DET_TIMINGS; i++) {
		if (nv_connector->edid->det_mon[i].type != DT)
			continue;
		/* Selecting only based on width ok? */
		if (nv_connector->edid->det_mon[i].section.d_timings.h_active > max_h_active) {
			max_h_active = nv_connector->edid->det_mon[i].section.d_timings.h_active;
			max_v_active = nv_connector->edid->det_mon[i].section.d_timings.v_active;
		}
	}
	if (!max_h_active || !max_v_active) /* what kind of a joke EDID is this? */
		for (i = 0; i < STD_TIMINGS; i++)
			if (nv_connector->edid->timings2[i].hsize > max_h_active) {
				max_h_active = nv_connector->edid->timings2[i].hsize;
				max_v_active = nv_connector->edid->timings2[i].vsize;
			}
	if (!max_h_active || !max_v_active) {
		NV_ERROR(output->scrn, "EDID too broken to find native mode\n");
		return NULL;
	}

	if (nv_encoder->native_mode) {
		xfree(nv_encoder->native_mode);
		nv_encoder->native_mode = NULL;
	}

	for (mode = edid_modes; mode != NULL; mode = mode->next) {
		if (mode->HDisplay == max_h_active &&
			mode->VDisplay == max_v_active) {
			/* Take the preferred mode when it exists. */
			if (mode->type & M_T_PREFERRED) {
				nv_encoder->native_mode = xf86DuplicateMode(mode);
				break;
			}
			/* Find the highest refresh mode otherwise. */
			if (!nv_encoder->native_mode || (mode->VRefresh > nv_encoder->native_mode->VRefresh)) {
				if (nv_encoder->native_mode)
					xfree(nv_encoder->native_mode);
				mode->type |= M_T_PREFERRED;
				nv_encoder->native_mode = xf86DuplicateMode(mode);
			}
		}
	}

	return nv_encoder->native_mode;
}

static DisplayModePtr
nv_output_get_edid_modes(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	enum nouveau_encoder_type enctype = nv_encoder->dcb->type;
	DisplayModePtr edid_modes;

	if (enctype == OUTPUT_LVDS ||
	    (enctype == OUTPUT_TMDS && nv_encoder->scaling_mode != SCALE_PANEL))
		/* the digital scaler is not limited to modes given in the EDID,
		 * so enable the GTF bit in order that the xserver thinks
		 * continuous timing is available and adds the standard modes
		 */
		nv_connector->edid->features.msc |= 1;

	xf86OutputSetEDID(output, nv_connector->edid);
	if (!(edid_modes = xf86OutputGetEDIDModes(output)))
		return edid_modes;

	if (enctype == OUTPUT_LVDS || enctype == OUTPUT_TMDS)
		if (!get_native_mode_from_edid(output, edid_modes))
			return NULL;
	if (enctype == OUTPUT_TMDS)
		nv_encoder->dual_link = nv_encoder->native_mode->Clock >= 165000;

	return edid_modes;
}

static DisplayModePtr
nv_lvds_output_get_modes(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	ScrnInfoPtr pScrn = output->scrn;
	DisplayModeRec mode, *ret_mode = NULL;
	int clock = 0;	/* needs to be zero for straps case */
	bool dl, if_is_24bit = false;

	/* panels only have one mode, and it doesn't change */
	if (nv_encoder->native_mode)
		return xf86DuplicateMode(nv_encoder->native_mode);

	if (nv_encoder->dcb->lvdsconf.use_straps_for_mode) {
		if (!nouveau_bios_fp_mode(pScrn, &mode))
			return NULL;

		mode.status = MODE_OK;
		mode.type = M_T_DRIVER | M_T_PREFERRED;
		xf86SetModeDefaultName(&mode);

		nv_encoder->native_mode = xf86DuplicateMode(&mode);
		ret_mode = xf86DuplicateMode(&mode);
	} else {
		ret_mode = nv_output_get_edid_modes(output);
		clock = nv_encoder->native_mode->Clock;
	}

	if (nouveau_bios_parse_lvds_table(pScrn, clock, &dl, &if_is_24bit))
		return NULL;

	/* because of the pre-existing native mode exit above, this will only
	 * get run at startup (and before create_resources is called in
	 * mode_fixup), so subsequent user dither settings are not overridden
	 */
	nv_encoder->dithering |= !if_is_24bit;
	nv_encoder->dual_link = dl;

	return ret_mode;
}

static int nv_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_connector(output)->detected_encoder;

	/* mode_valid can be called by someone doing addmode on an output
	 * which is disconnected and so without an encoder; avoid crashing
	 */
	if (!nv_encoder)
		return MODE_ERROR;

	if (!output->doubleScanAllowed && mode->Flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;
	if (!output->interlaceAllowed && mode->Flags & V_INTERLACE)
		return MODE_NO_INTERLACE;

	if (nv_encoder->dcb->type == OUTPUT_ANALOG) {
		if (nv_encoder->dcb->crtconf.maxfreq) {
			if (mode->Clock > nv_encoder->dcb->crtconf.maxfreq)
				return MODE_CLOCK_HIGH;
		} else
			if (mode->Clock > 350000)
				return MODE_CLOCK_HIGH;
	}
	if (nv_encoder->dcb->type == OUTPUT_LVDS || nv_encoder->dcb->type == OUTPUT_TMDS)
		/* No modes > panel's native res */
		if (mode->HDisplay > nv_encoder->native_mode->HDisplay ||
		    mode->VDisplay > nv_encoder->native_mode->VDisplay)
			return MODE_PANEL;
	if (nv_encoder->dcb->type == OUTPUT_TMDS) {
		if (nv_encoder->dcb->duallink_possible) {
			if (mode->Clock > 330000) /* 2x165 MHz */
				return MODE_CLOCK_HIGH;
		} else
			if (mode->Clock > 165000) /* 165 MHz */
				return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static void
nv_output_destroy(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder;
	NVPtr pNv = NVPTR(output->scrn);
	int i;

	if (!nv_connector)
		return;

	if (nv_connector->edid)
		xfree(nv_connector->edid);
	FOR_EACH_ENCODER_IN_CONNECTOR(i, nv_connector, nv_encoder)
		if (nv_encoder->native_mode)
			xfree(nv_encoder->native_mode);
	xfree(nv_connector);
}

static char * get_current_scaling_name(enum scaling_modes mode)
{
	static const struct {
		char *name;
		enum scaling_modes mode;
	} scaling_mode[] = {
		{ "panel", SCALE_PANEL },
		{ "fullscreen", SCALE_FULLSCREEN },
		{ "aspect", SCALE_ASPECT },
		{ "noscale", SCALE_NOSCALE },
		{ NULL, SCALE_INVALID }
	};
	int i;

	for (i = 0; scaling_mode[i].name; i++)
		if (scaling_mode[i].mode == mode)
			return scaling_mode[i].name;

	return NULL;
}

static int nv_output_create_prop(xf86OutputPtr output, char *name, Atom *atom,
				 INT32 *rangevals, INT32 cur_val, char *cur_str, Bool do_mode_set)
{
	int ret = -ENOMEM;
	Bool range = rangevals ? TRUE : FALSE;

	if ((*atom = MakeAtom(name, strlen(name), TRUE)) == BAD_RESOURCE)
		goto fail;
	if (RRQueryOutputProperty(output->randr_output, *atom))
		return 0;	/* already exists */
	if ((ret = RRConfigureOutputProperty(output->randr_output, *atom,
					     do_mode_set, range, FALSE, range ? 2 : 0, rangevals)))
		goto fail;
	if (range)
		ret = RRChangeOutputProperty(output->randr_output, *atom, XA_INTEGER, 32,
					     PropModeReplace, 1, &cur_val, FALSE, do_mode_set);
	else
		ret = RRChangeOutputProperty(output->randr_output, *atom, XA_STRING, 8,
					     PropModeReplace, strlen(cur_str), cur_str,
					     FALSE, do_mode_set);

fail:
	if (ret)
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
			   "Creation of %s property failed: %d\n", name, ret);

	return ret;
}

static Atom dithering_atom, scaling_mode_atom;
static Atom dv_atom, sharpness_atom;

static void nv_output_create_resources(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	NVPtr pNv = NVPTR(output->scrn);

	/* may be called before encoder is picked, resources will be created
	 * by update_output_fields()
	 */
	if (!nv_encoder)
		return;

	if (nv_encoder->dcb->type == OUTPUT_LVDS || nv_encoder->dcb->type == OUTPUT_TMDS) {
		nv_output_create_prop(output, "DITHERING", &dithering_atom,
				      (INT32 []){ 0, 1 }, nv_encoder->dithering, NULL, TRUE);
		nv_output_create_prop(output, "SCALING_MODE", &scaling_mode_atom,
				      NULL, 0, get_current_scaling_name(nv_encoder->scaling_mode), TRUE);
	}
	if (pNv->NVArch >= 0x11 && output->crtc) {
		struct nouveau_crtc *nv_crtc = to_nouveau_crtc(output->crtc);
		INT32 dv_range[2] = { 0, !pNv->gf4_disp_arch ? 3 : 63 };
		/* unsure of correct condition here: blur works on my nv34, but not on my nv31 */
		INT32 is_range[2] = { pNv->NVArch > 0x31 ? -32 : 0, 31 };

		nv_output_create_prop(output, "DIGITAL_VIBRANCE", &dv_atom,
				      dv_range, nv_crtc->saturation, NULL, FALSE);
		if (pNv->NVArch >= 0x30)
			nv_output_create_prop(output, "IMAGE_SHARPENING", &sharpness_atom,
					      is_range, nv_crtc->sharpness, NULL, FALSE);
	}
}

static Bool
nv_output_set_property(xf86OutputPtr output, Atom property,
				RRPropertyValuePtr value)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	NVPtr pNv = NVPTR(output->scrn);

	if (property == scaling_mode_atom) {
		int32_t ret;
		char *name = NULL;

		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		name = (char *) value->data;

		/* Match a string to a scaling mode */
		ret = nv_scaling_mode_lookup(name, value->size);
		if (ret == SCALE_INVALID)
			return FALSE;

		/* LVDS must always use gpu scaling. */
		if (ret == SCALE_PANEL && nv_encoder->dcb->type == OUTPUT_LVDS)
			return FALSE;

		nv_encoder->scaling_mode = ret;
	} else if (property == dithering_atom) {
		if (value->type != XA_INTEGER || value->format != 32)
			return FALSE;

		int32_t val = *(int32_t *) value->data;

		if (val < 0 || val > 1)
			return FALSE;

		nv_encoder->dithering = val;
	} else if (property == dv_atom || property == sharpness_atom) {
		int32_t val = *(int32_t *) value->data;

		if (!output->crtc)
			return FALSE;
		if (value->type != XA_INTEGER || value->format != 32)
			return FALSE;

		if (property == dv_atom) {
			if (val < 0 || val > (!pNv->gf4_disp_arch ? 3 : 63))
				return FALSE;

			nv_crtc_set_digital_vibrance(output->crtc, val);
		} else {
			if (val < (pNv->NVArch > 0x31 ? -32 : 0) || val > 31)
				return FALSE;

			nv_crtc_set_image_sharpening(output->crtc, val);
		}
	}

	return TRUE;
}

static Bool
nv_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);

	if (nv_connector->nv_encoder != nv_connector->detected_encoder) {
		nv_connector->nv_encoder = nv_connector->detected_encoder;
		if (output->randr_output) {
			RRDeleteOutputProperty(output->randr_output, dithering_atom);
			RRDeleteOutputProperty(output->randr_output, scaling_mode_atom);
			output->funcs->create_resources(output);
		}
	}

	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);

	/* For internal panels and gpu scaling on DVI we need the native mode */
	if (nv_encoder->dcb->type == OUTPUT_LVDS ||
	    (nv_encoder->dcb->type == OUTPUT_TMDS && nv_encoder->scaling_mode != SCALE_PANEL)) {
		adjusted_mode->HDisplay = nv_encoder->native_mode->HDisplay;
		adjusted_mode->HSkew = nv_encoder->native_mode->HSkew;
		adjusted_mode->HSyncStart = nv_encoder->native_mode->HSyncStart;
		adjusted_mode->HSyncEnd = nv_encoder->native_mode->HSyncEnd;
		adjusted_mode->HTotal = nv_encoder->native_mode->HTotal;
		adjusted_mode->VDisplay = nv_encoder->native_mode->VDisplay;
		adjusted_mode->VScan = nv_encoder->native_mode->VScan;
		adjusted_mode->VSyncStart = nv_encoder->native_mode->VSyncStart;
		adjusted_mode->VSyncEnd = nv_encoder->native_mode->VSyncEnd;
		adjusted_mode->VTotal = nv_encoder->native_mode->VTotal;
		adjusted_mode->Clock = nv_encoder->native_mode->Clock;
		adjusted_mode->Flags = nv_encoder->native_mode->Flags;

		xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
	}

	return TRUE;
}

static void nv_digital_output_prepare_sel_clk(NVPtr pNv, struct nouveau_encoder *nv_encoder, int head)
{
	NVRegPtr state = &pNv->ModeReg;
	uint32_t bits1618 = nv_encoder->dcb->or & OUTPUT_A ? 0x10000 : 0x40000;

	if (nv_encoder->dcb->location != DCB_LOC_ON_CHIP)
		return;

	/* SEL_CLK is only used on the primary ramdac
	 * It toggles spread spectrum PLL output and sets the bindings of PLLs
	 * to heads on digital outputs
	 */
	if (head)
		state->sel_clk |= bits1618;
	else
		state->sel_clk &= ~bits1618;

	/* nv30:
	 *	bit 0		NVClk spread spectrum on/off
	 *	bit 2		MemClk spread spectrum on/off
	 * 	bit 4		PixClk1 spread spectrum on/off toggle
	 * 	bit 6		PixClk2 spread spectrum on/off toggle
	 *
	 * nv40 (observations from bios behaviour and mmio traces):
	 * 	bits 4&6	as for nv30
	 * 	bits 5&7	head dependent as for bits 4&6, but do not appear with 4&6;
	 * 			maybe a different spread mode
	 * 	bits 8&10	seen on dual-link dvi outputs, purpose unknown (set by POST scripts)
	 * 	The logic behind turning spread spectrum on/off in the first place,
	 * 	and which bit-pair to use, is unclear on nv40 (for earlier cards, the fp table
	 * 	entry has the necessary info)
	 */
	if (nv_encoder->dcb->type == OUTPUT_LVDS && pNv->SavedReg.sel_clk & 0xf0) {
		int shift = (pNv->SavedReg.sel_clk & 0x50) ? 0 : 1;

		state->sel_clk &= ~0xf0;
		state->sel_clk |= (head ? 0x40 : 0x10) << shift;
	}
}

#define FP_TG_CONTROL_ON  (NV_PRAMDAC_FP_TG_CONTROL_DISPEN_POS |	\
			   NV_PRAMDAC_FP_TG_CONTROL_HSYNC_POS |		\
			   NV_PRAMDAC_FP_TG_CONTROL_VSYNC_POS)
#define FP_TG_CONTROL_OFF (NV_PRAMDAC_FP_TG_CONTROL_DISPEN_DISABLE |	\
			   NV_PRAMDAC_FP_TG_CONTROL_HSYNC_DISABLE |	\
			   NV_PRAMDAC_FP_TG_CONTROL_VSYNC_DISABLE)

static bool is_fpc_off(uint32_t fpc)
{
	return ((fpc & (FP_TG_CONTROL_ON | FP_TG_CONTROL_OFF)) ==
							FP_TG_CONTROL_OFF);
}

static void
nv_output_prepare(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	NVPtr pNv = NVPTR(output->scrn);
	int head = to_nouveau_crtc(output->crtc)->head;
	struct nv_crtc_reg *crtcstate = pNv->ModeReg.crtc_reg;
	uint8_t *cr_lcd = &crtcstate[head].CRTC[NV_CIO_CRE_LCD__INDEX];
	uint8_t *cr_lcd_oth = &crtcstate[head ^ 1].CRTC[NV_CIO_CRE_LCD__INDEX];
	bool digital_op = nv_encoder->dcb->type == OUTPUT_LVDS ||
			  nv_encoder->dcb->type == OUTPUT_TMDS;

	output->funcs->dpms(output, DPMSModeOff);

	if (nv_encoder->dcb->type == OUTPUT_ANALOG) {
		if (NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_TG_CONTROL) &
							FP_TG_CONTROL_ON) {
			/* digital remnants must be cleaned before new crtc
			 * values programmed.  delay is time for the vga stuff
			 * to realise it's in control again
			 */
			NVWriteRAMDAC(pNv, head, NV_PRAMDAC_FP_TG_CONTROL,
				      FP_TG_CONTROL_OFF);
			usleep(50000);
		}
		/* don't inadvertently turn it on when state written later */
		crtcstate[head].fp_control = FP_TG_CONTROL_OFF;
	}

	/* calculate some output specific CRTC regs now, so that they can be
	 * written in nv_crtc_set_mode
	 */

	if (digital_op)
		nv_digital_output_prepare_sel_clk(pNv, nv_encoder, head);

	/* Some NV4x have unknown values (0x3f, 0x50, 0x54, 0x6b, 0x79, 0x7f)
	 * at LCD__INDEX which we don't alter
	 */
	if (!(*cr_lcd & 0x44)) {
		*cr_lcd = digital_op ? 0x3 : 0x0;
		if (digital_op && pNv->twoHeads) {
			if (nv_encoder->dcb->location == DCB_LOC_ON_CHIP)
				*cr_lcd |= head ? 0x0 : 0x8;
			else {
				*cr_lcd |= (nv_encoder->dcb->or << 4) & 0x30;
				if (nv_encoder->dcb->type == OUTPUT_LVDS)
					*cr_lcd |= 0x30;
				if ((*cr_lcd & 0x30) == (*cr_lcd_oth & 0x30)) {
					/* avoid being connected to both crtcs */
					*cr_lcd_oth &= ~0x30;
					NVWriteVgaCrtc(pNv, head ^ 1,
						       NV_CIO_CRE_LCD__INDEX,
						       *cr_lcd_oth);
				}
			}
		}
	}
}

static void
nv_output_mode_set(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct dcb_entry *dcbe = nv_encoder->dcb;
	int head = to_nouveau_crtc(output->crtc)->head;

	NV_TRACE(pScrn, "%s called for encoder %d\n", __func__, dcbe->index);

	if (pNv->gf4_disp_arch && dcbe->type == OUTPUT_ANALOG) {
		uint32_t dac_offset = nv_output_ramdac_offset(nv_encoder);
		uint32_t otherdac;
		int i;

		/* bit 16-19 are bits that are set on some G70 cards,
		 * but don't seem to have much effect */
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + dac_offset,
			      head << 8 | NV_PRAMDAC_DACCLK_SEL_DACCLK);
		/* force any other vga encoders to bind to the other crtc */
		for (i = 0; i < pNv->vbios->dcb->entries; i++)
			if (i != dcbe->index && pNv->encoders[i].dcb &&
			    pNv->encoders[i].dcb->type == OUTPUT_ANALOG) {
				dac_offset = nv_output_ramdac_offset(&pNv->encoders[i]);
				otherdac = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + dac_offset);
				NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + dac_offset,
					      (otherdac & ~0x100) | (head ^ 1) << 8);
			}
	}
	if (dcbe->type == OUTPUT_TMDS)
		run_tmds_table(pScrn, dcbe, head, adjusted_mode->Clock);
	else if (dcbe->type == OUTPUT_LVDS)
		call_lvds_script(pScrn, dcbe, head, LVDS_RESET, adjusted_mode->Clock);
	if (dcbe->type == OUTPUT_LVDS || dcbe->type == OUTPUT_TMDS)
		/* update fp_control state for any changes made by scripts,
		 * so correct value is written at DPMS on */
		pNv->ModeReg.crtc_reg[head].fp_control =
			NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_TG_CONTROL);

	/* This could use refinement for flatpanels, but it should work this way */
	if (pNv->NVArch < 0x44)
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + nv_output_ramdac_offset(nv_encoder), 0xf0000000);
	else
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_TEST_CONTROL + nv_output_ramdac_offset(nv_encoder), 0x00100000);
}

static void
nv_output_commit(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(output->crtc);

	output->funcs->dpms(output, DPMSModeOn);

	NV_TRACE(pScrn, "Output %s is running on CRTC %d using output %c\n",
		 output->name, nv_crtc->head, '@' + ffs(nv_encoder->dcb->or));
}

static void dpms_update_fp_control(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc;
	uint32_t *fpc;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	if (mode == DPMSModeOn) {
		nv_crtc = to_nouveau_crtc(crtc);
		fpc = &pNv->ModeReg.crtc_reg[nv_crtc->head].fp_control;

		if (is_fpc_off(*fpc))
			/* using saved value is ok, as (is_digital && dpms_on &&
			 * fp_control==OFF) is (at present) *only* true when
			 * fpc's most recent change was by below "off" code
			 */
			*fpc = nv_crtc->dpms_saved_fp_control;

		nv_crtc->fp_users |= 1 << nv_encoder->dcb->index;
		NVWriteRAMDAC(pNv, nv_crtc->head, NV_PRAMDAC_FP_TG_CONTROL, *fpc);
	} else
		for (i = 0; i < xf86_config->num_crtc; i++) {
			nv_crtc = to_nouveau_crtc(xf86_config->crtc[i]);
			fpc = &pNv->ModeReg.crtc_reg[nv_crtc->head].fp_control;

			nv_crtc->fp_users &= ~(1 << nv_encoder->dcb->index);
			if (!is_fpc_off(*fpc) && !nv_crtc->fp_users) {
				nv_crtc->dpms_saved_fp_control = *fpc;
				/* cut the FP output */
				*fpc &= ~FP_TG_CONTROL_ON;
				*fpc |= FP_TG_CONTROL_OFF;
				NVWriteRAMDAC(pNv, nv_crtc->head,
					      NV_PRAMDAC_FP_TG_CONTROL, *fpc);
			}
		}
}

static bool is_powersaving_dpms(int mode)
{
	return (mode == DPMSModeStandby || mode == DPMSModeSuspend ||
							mode == DPMSModeOff);
}

static void
lvds_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);
	bool was_powersaving = is_powersaving_dpms(nv_encoder->last_dpms);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	NV_TRACE(pScrn, "Setting dpms mode %d on lvds encoder (output %d)\n",
		 mode, nv_encoder->dcb->index);

	if (was_powersaving && is_powersaving_dpms(mode))
		return;

	if (nv_encoder->dcb->lvdsconf.use_power_scripts) {
		/* when removing an output, crtc may not be set, but PANEL_OFF
		 * must still be run
		 */
		int head = crtc ? to_nouveau_crtc(crtc)->head :
			   nv_get_digital_bound_head(pNv, nv_encoder->dcb->or);

		if (mode == DPMSModeOn)
			call_lvds_script(pScrn, nv_encoder->dcb, head,
					 LVDS_PANEL_ON, nv_encoder->native_mode->Clock);
		else
			/* pxclk of 0 is fine for PANEL_OFF, and for a
			 * disconnected LVDS encoder there is no native_mode
			 */
			call_lvds_script(pScrn, nv_encoder->dcb, head,
					 LVDS_PANEL_OFF, 0);
	}

	dpms_update_fp_control(pScrn, nv_encoder, crtc, mode);

	if (mode == DPMSModeOn)
		nv_digital_output_prepare_sel_clk(pNv, nv_encoder, to_nouveau_crtc(crtc)->head);
	else {
		pNv->ModeReg.sel_clk = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_SEL_CLK);
		pNv->ModeReg.sel_clk &= ~0xf0;
	}
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_SEL_CLK, pNv->ModeReg.sel_clk);
}

static void
vga_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	NV_TRACE(pScrn, "Setting dpms mode %d on vga encoder (output %d)\n",
		 mode, nv_encoder->dcb->index);

	if (pNv->gf4_disp_arch) {
		uint32_t outputval = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + nv_output_ramdac_offset(nv_encoder));

		if (mode == DPMSModeOff)
			NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + nv_output_ramdac_offset(nv_encoder),
				      outputval & ~NV_PRAMDAC_DACCLK_SEL_DACCLK);
		else if (mode == DPMSModeOn)
			NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK + nv_output_ramdac_offset(nv_encoder),
				      outputval | NV_PRAMDAC_DACCLK_SEL_DACCLK);
	}
}

static void
tmds_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	NV_TRACE(pScrn, "Setting dpms mode %d on tmds encoder (output %d)\n",
		 mode, nv_encoder->dcb->index);

	dpms_update_fp_control(pScrn, nv_encoder, crtc, mode);
}

static void nv_output_dpms(xf86OutputPtr output, int mode)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcPtr crtc = output->crtc;
	NVPtr pNv = NVPTR(pScrn);
	int i;
	void (* const encoder_dpms[4])(ScrnInfoPtr, struct nouveau_encoder *, xf86CrtcPtr, int) =
		/* index matches DCB type */
		{ vga_encoder_dpms, NULL, tmds_encoder_dpms, lvds_encoder_dpms };

	struct nouveau_encoder *nv_encoder_i;
	FOR_EACH_ENCODER_IN_CONNECTOR(i, nv_connector, nv_encoder_i)
		if (nv_encoder_i != nv_encoder)
			encoder_dpms[nv_encoder_i->dcb->type](pScrn, nv_encoder_i, crtc, DPMSModeOff);

	if (nv_encoder) /* may be called before encoder is picked, but iteration above solves it */
		encoder_dpms[nv_encoder->dcb->type](pScrn, nv_encoder, crtc, mode);
}

void nv_encoder_save(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!nv_encoder->dcb)	/* uninitialised encoder */
		return;

	if (pNv->gf4_disp_arch && nv_encoder->dcb->type == OUTPUT_ANALOG)
		nv_encoder->restore.output =
			NVReadRAMDAC(pNv, 0, NV_PRAMDAC_DACCLK +
				nv_output_ramdac_offset(nv_encoder));
	if (pNv->twoHeads && (nv_encoder->dcb->type == OUTPUT_LVDS ||
			      nv_encoder->dcb->type == OUTPUT_TMDS))
		nv_encoder->restore.head =
			nv_get_digital_bound_head(pNv, nv_encoder->dcb->or);
}

void nv_encoder_restore(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);
	int head = nv_encoder->restore.head;

	if (!nv_encoder->dcb)	/* uninitialised encoder */
		return;

	if (pNv->gf4_disp_arch && nv_encoder->dcb->type == OUTPUT_ANALOG)
		NVWriteRAMDAC(pNv, 0,
			      NV_PRAMDAC_DACCLK + nv_output_ramdac_offset(nv_encoder),
			      nv_encoder->restore.output);
	if (nv_encoder->dcb->type == OUTPUT_LVDS)
		call_lvds_script(pScrn, nv_encoder->dcb, head, LVDS_PANEL_ON,
				 nv_encoder->native_mode->Clock);
	if (nv_encoder->dcb->type == OUTPUT_TMDS) {
		int clock = nouveau_hw_pllvals_to_clk
					(&pNv->SavedReg.crtc_reg[head].pllvals);

		run_tmds_table(pScrn, nv_encoder->dcb, head, clock);
	}

	nv_encoder->last_dpms = NV_DPMS_CLEARED;
}

static const xf86OutputFuncsRec nv_output_funcs = {
	.dpms = nv_output_dpms,
	.mode_valid = nv_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_output_detect,
	.get_modes = nv_output_get_edid_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
	.create_resources = nv_output_create_resources,
	.set_property = nv_output_set_property,
};

static const xf86OutputFuncsRec nv_lvds_output_funcs = {
	.dpms = nv_output_dpms,
	.mode_valid = nv_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_output_detect,
	.get_modes = nv_lvds_output_get_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
	.create_resources = nv_output_create_resources,
	.set_property = nv_output_set_property,
};

static void
nv_add_encoder(ScrnInfoPtr pScrn, struct dcb_entry *dcbent)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_encoder *nv_encoder = &pNv->encoders[dcbent->index];

	nv_encoder->dcb = dcbent;
	nv_encoder->last_dpms = NV_DPMS_CLEARED;
	nv_encoder->dithering = pNv->FPDither;
	if (pNv->fpScaler) /* GPU Scaling */
		nv_encoder->scaling_mode = SCALE_ASPECT;
	else if (nv_encoder->dcb->type == OUTPUT_LVDS)
		nv_encoder->scaling_mode = SCALE_NOSCALE;
	else
		nv_encoder->scaling_mode = SCALE_PANEL;
	if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
		nv_encoder->scaling_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
		if (nv_encoder->scaling_mode == SCALE_INVALID)
			nv_encoder->scaling_mode = SCALE_ASPECT; /* default */
	}
}

static void
nv_add_connector(ScrnInfoPtr pScrn, int i2c_index, int encoders, const xf86OutputFuncsRec *output_funcs, char *outputname)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr output;
	struct nouveau_connector *nv_connector;

	if (!(output = xf86OutputCreate(pScrn, output_funcs, outputname)))
		return;
	if (!(nv_connector = xcalloc(1, sizeof (struct nouveau_connector)))) {
		xf86OutputDestroy(output);
		return;
	}

	output->driver_private = nv_connector;

	if (i2c_index < 0xf)
		NV_I2CInit(pScrn, &nv_connector->pDDCBus, &pNv->vbios->dcb->i2c[i2c_index], xstrdup(outputname));
	nv_connector->possible_encoders = encoders;
}

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct parsed_dcb *dcb = pNv->vbios->dcb;
	uint16_t connectors[0x10] = { 0 };
	int i, vga_count = 0, dvid_count = 0, dvii_count = 0, lvds_count = 0;

	if (!(pNv->encoders = xcalloc(dcb->entries, sizeof (struct nouveau_encoder))))
		return;

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];

		if (dcbent->type == OUTPUT_TV)
			continue;
		if (dcbent->type > 3) {
			NV_WARN(pScrn, "DCB type %d not known\n", dcbent->type);
			continue;
		}

		connectors[dcbent->i2c_index] |= 1 << i;

		nv_add_encoder(pScrn, dcbent);
	}

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];
		int i2c_index = dcbent->i2c_index;
		uint16_t encoders = connectors[i2c_index];
		char outputname[20];
		xf86OutputFuncsRec const *funcs = &nv_output_funcs;

		if (!encoders)
			continue;

		switch (dcbent->type) {
		case OUTPUT_ANALOG:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(outputname, "VGA-%d", vga_count++);
			else
				sprintf(outputname, "DVI-I-%d", dvii_count++);
			break;
		case OUTPUT_TMDS:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(outputname, "DVI-D-%d", dvid_count++);
			else
				sprintf(outputname, "DVI-I-%d", dvii_count++);
			break;
		case OUTPUT_LVDS:
			sprintf(outputname, "LVDS-%d", lvds_count++);
			funcs = &nv_lvds_output_funcs;
			/* don't create i2c adapter when lvds ddc not allowed */
			if (dcbent->lvdsconf.use_straps_for_mode ||
			    pNv->vbios->fp_no_ddc)
				i2c_index = 0xf;
			break;
		default:
			continue;
		}

		nv_add_connector(pScrn, i2c_index, encoders, funcs, outputname);
		connectors[i2c_index] = 0; /* avoid connectors being added multiply */
	}
}
