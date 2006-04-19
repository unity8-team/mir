/**************************************************************************

 Copyright 2006 Dave Airlie <airlied@linux.ie>
 
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "i830.h"
#include "i830_sdvo_regs.h"

CARD16 curr_table[6];

/* SDVO support for i9xx chipsets */
static Bool sReadByte(I830SDVOPtr s, int addr, unsigned char *ch)
{
    if (!xf86I2CReadByte(&s->d, addr, ch)) {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read from %s Slave %d.\n", s->d.pI2CBus->BusName,
		   s->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

static Bool sWriteByte(I830SDVOPtr s, int addr, unsigned char ch)
{
    if (!xf86I2CWriteByte(&s->d, addr, ch)) {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n", s->d.pI2CBus->BusName,
		   s->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/* following on from tracing the intel BIOS i2c routines */
static void
I830SDVOWriteOutputs(I830SDVOPtr s, int num_out)
{
    int i;

    ErrorF("SDVO: W: ");
    for (i = num_out; i <= SDVO_I2C_ARG_0; i++)
	ErrorF("%02X ", s->sdvo_regs[i]);
    ErrorF("\n");

    /* blast the output regs */
    for (i = SDVO_I2C_ARG_0; i >= num_out; i--) {
	sWriteByte(s, i, s->sdvo_regs[i]);
    }
    /* blast the command reg */
    sWriteByte(s, SDVO_I2C_OPCODE, s->sdvo_regs[SDVO_I2C_OPCODE]);
}

static void
I830SDVOReadInputRegs(I830SDVOPtr s)
{
    int i;

    /* follow BIOS ordering */
    sReadByte(s, SDVO_I2C_CMD_STATUS, &s->sdvo_regs[SDVO_I2C_CMD_STATUS]);
  
    sReadByte(s, SDVO_I2C_RETURN_3, &s->sdvo_regs[SDVO_I2C_RETURN_3]);
    sReadByte(s, SDVO_I2C_RETURN_2, &s->sdvo_regs[SDVO_I2C_RETURN_2]);
    sReadByte(s, SDVO_I2C_RETURN_1, &s->sdvo_regs[SDVO_I2C_RETURN_1]);
    sReadByte(s, SDVO_I2C_RETURN_0, &s->sdvo_regs[SDVO_I2C_RETURN_0]);
    sReadByte(s, SDVO_I2C_RETURN_7, &s->sdvo_regs[SDVO_I2C_RETURN_7]);
    sReadByte(s, SDVO_I2C_RETURN_6, &s->sdvo_regs[SDVO_I2C_RETURN_6]);
    sReadByte(s, SDVO_I2C_RETURN_5, &s->sdvo_regs[SDVO_I2C_RETURN_5]);
    sReadByte(s, SDVO_I2C_RETURN_4, &s->sdvo_regs[SDVO_I2C_RETURN_4]);

    ErrorF("SDVO: R: ");
    for (i = SDVO_I2C_CMD_STATUS; i <= SDVO_I2C_RETURN_7; i++)
	ErrorF("%02X ", s->sdvo_regs[i]);
    ErrorF("\n");
}

Bool
I830SDVOSetupDDC(I830SDVOPtr s, int enable)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CONTROL_BUS_SWITCH;
    s->sdvo_regs[SDVO_I2C_ARG_0] = SDVO_CONTROL_BUS_DDC2;

    I830SDVOWriteOutputs(s, 7);

    sReadByte(s, SDVO_I2C_CMD_STATUS, &s->sdvo_regs[SDVO_I2C_CMD_STATUS]);

    ErrorF("SDVO: R: ");
    ErrorF("%02X ", s->sdvo_regs[SDVO_I2C_CMD_STATUS]);
    ErrorF("\n");
    return TRUE;
}

static Bool
I830SDVOSetTargetInput(I830SDVOPtr s)
{
    /* write out 0x10 */
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_INPUT;

    I830SDVOWriteOutputs(s, 0);

    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetTrainedInputs(I830SDVOPtr s, int on)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_TRAINED_INPUTS;

    /* XXX: I don't believe we need to set anything here --anholt */
    s->sdvo_regs[0x07] = on ? 0x80 : 0x00;
    s->sdvo_regs[0x04] = on ? 0x80 : 0x00;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetActiveOutputs(I830SDVOPtr s, int on)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ACTIVE_OUTPUTS;

    s->sdvo_regs[0x07] = on ? 0x01 : 0x00;
    s->sdvo_regs[0x03] = 0x1;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOSetActiveOutputs(I830SDVOPtr s, int on)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_ACTIVE_OUTPUTS;

    /* XXX: This should be touching args 0,1, I believe.  --anholt */
    s->sdvo_regs[0x07] = on ? 0x01 : 0x00;
    s->sdvo_regs[0x03] = on ? 0x01 : 0x00;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetInputPixelClockRange(I830SDVOPtr s, CARD16 clock, CARD16 height)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE;

    /* XXX: SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE shouldn't be taking args. */

    /* set clock regs */
    s->sdvo_regs[0x06] = (clock >> 8) & 0xff;
    s->sdvo_regs[0x07] = clock & 0xff;

    /* set height regs */
    s->sdvo_regs[0x02] = (height >> 8) & 0xff;
    s->sdvo_regs[0x03] = height & 0xff;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOSetTargetOutput(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_OUTPUT;

    s->sdvo_regs[SDVO_I2C_ARG_0] = 0x1;	/* Enable */
    s->sdvo_regs[SDVO_I2C_ARG_1] = 0x0; /* Disable */

    I830SDVOWriteOutputs(s, 0);		/* XXX: Only write these two */
    I830SDVOReadInputRegs(s);

    return TRUE;
}

#if 0
static Bool
I830SDVOGetOutputTimingsPart1(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_OUTPUT_TIMINGS_PART1;

    /* XXX: No args */
    s->sdvo_regs[0x07] = 0x0;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);
  
    return TRUE;
}

static Bool
I830SDVOGetOutputTimingsPart2(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_OUTPUT_TIMINGS_PART2;

    /* XXX: No args */
    s->sdvo_regs[0x07] = 0x0;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}
#endif

static Bool
I830SDVOSetTimingsPart1(I830SDVOPtr s, char cmd, CARD16 clock, CARD16 magic1,
			CARD16 magic2, CARD16 magic3)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;

    /* set clock regs */
    s->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_2] = magic3 & 0xff;  
    s->sdvo_regs[SDVO_I2C_ARG_3] = (magic3 >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_4] = magic2 & 0xff;  
    s->sdvo_regs[SDVO_I2C_ARG_5] = (magic2 >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_6] = magic1 & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_7] = (magic1 >> 8) & 0xff;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);
  
    return TRUE;
}

static Bool
I830SDVOSetInputTimingsPart1(I830SDVOPtr s, CARD16 clock,
			     CARD16 magic1, CARD16 magic2, CARD16 magic3)
{
    return I830SDVOSetTimingsPart1(s, SDVO_CMD_SET_INPUT_TIMINGS_PART1,
				   clock, magic1, magic2, magic3);
}

static Bool
I830SDVOSetOutputTimingsPart1(I830SDVOPtr s, CARD16 clock, CARD16 magic1,
			      CARD16 magic2, CARD16 magic3)
{
    return I830SDVOSetTimingsPart1(s, SDVO_CMD_SET_OUTPUT_TIMINGS_PART1,
				   clock, magic1, magic2, magic3);
}

static Bool
I830SDVOGetPreferredInputTimingPart2(I830SDVOPtr s, CARD16 clock,
				     CARD16 magic1, CARD16 magic2,
				     CARD16 magic3)
{
    Bool ok;

    /* XXX: This is a rather different command */
    ok = I830SDVOSetTimingsPart1(s, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2,
				 clock, magic1, magic2, magic3);

    curr_table[3] = s->sdvo_regs[SDVO_I2C_RETURN_0] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    curr_table[4] = s->sdvo_regs[SDVO_I2C_RETURN_2] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_3] << 8);
    curr_table[5] = 0x1e;

    return ok;
}

static Bool
I830SDVOSetTimingsPart2(I830SDVOPtr s, CARD8 cmd, CARD16 magic4, CARD16 magic5,
			CARD16 magic6)
{
    memset(s->sdvo_regs, 0, 9);
  
    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
 
    /* set clock regs */
    s->sdvo_regs[SDVO_I2C_ARG_0] = magic4 & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_1] = (magic4 >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_2] = magic5 & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_3] = (magic5 >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_4] = magic6 & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_5] = (magic6 >> 8) & 0xff;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);
  
    return TRUE;
}

static Bool
I830SDVOSetInputTimingsPart2(I830SDVOPtr s, CARD16 magic4, CARD16 magic5,
			     CARD16 magic6)
{
    return I830SDVOSetTimingsPart2(s, SDVO_CMD_SET_INPUT_TIMINGS_PART2, magic4,
				   magic5, magic6);
}

static Bool
I830SDVOSetOutputTimingsPart2(I830SDVOPtr s, CARD16 magic4, CARD16 magic5,
			      CARD16 magic6)
{
    return I830SDVOSetTimingsPart2(s, SDVO_CMD_SET_OUTPUT_TIMINGS_PART2, magic4,
				   magic5, magic6);
}

static Bool
I830SDVOCreatePreferredInputTiming(I830SDVOPtr s, CARD16 clock, CARD16 width,
				   CARD16 height)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING;

    s->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_2] = width & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_3] = (width >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_4] = (height >> 8) & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_5] = height & 0xff;  

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetPreferredInputTimingPart1(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    curr_table[0] = s->sdvo_regs[SDVO_I2C_RETURN_6] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_7] << 8);
    curr_table[1] = s->sdvo_regs[SDVO_I2C_RETURN_4] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_5] << 8);
    curr_table[2] = s->sdvo_regs[SDVO_I2C_RETURN_2] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_3] << 8);

    return TRUE;
}

static Bool
I830SDVOSetClockRateMult(I830SDVOPtr s, CARD8 val)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CLOCK_RATE_MULT;

    s->sdvo_regs[SDVO_I2C_ARG_0] = val;
    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

Bool
I830SDVOPreSetMode(I830SDVOPtr s, DisplayModePtr mode)
{
    CARD16 clock = mode->Clock/10, width = mode->CrtcHDisplay;
    CARD16 height = mode->CrtcVDisplay;
    CARD16 h_blank_len, h_sync_len, v_blank_len, v_sync_len;
    CARD16 h_sync_offset, v_sync_offset;
    CARD16 sync_flags;
    CARD8 c16a[8];
    CARD8 c17a[8];
    CARD16 out_timings[6];

    /* do some mode translations */
    h_blank_len = mode->CrtcHBlankEnd - mode->CrtcHBlankStart;
    h_sync_len = mode->CrtcHSyncEnd - mode->CrtcHSyncStart;

    v_blank_len = mode->CrtcVBlankEnd - mode->CrtcVBlankStart;
    v_sync_len = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;

    h_sync_offset = mode->CrtcHSyncStart - mode->CrtcHBlankStart;
    v_sync_offset = mode->CrtcVSyncStart - mode->CrtcVBlankStart;

    sync_flags = 0x18;
    if (mode->Flags & V_PHSYNC)
	sync_flags |= 0x2;
    if (mode->Flags & V_PVSYNC)
	sync_flags |= 0x4;
    /* high bits of 0 */
    c16a[7] = clock & 0xff;
    c16a[6] = (clock >> 8) & 0xff;
    c16a[5] = (width & 0xff);
    c16a[4] = (h_blank_len & 0xff);
    c16a[3] = (((width >> 8) & 0xf) << 4) | ((h_blank_len >> 8) & 0xf);
    c16a[2] = (height & 0xff);
    c16a[1] = (v_blank_len & 0xff);
    c16a[0] = (((height >> 8) & 0xf) << 4) | ((v_blank_len >> 8) & 0xf);

    c17a[7] = h_sync_offset;
    c17a[6] = h_sync_len & 0xff;
    c17a[5] = (v_sync_offset & 0xf) << 4 | (v_sync_len & 0xf);
    c17a[4] = 0;
    c17a[3] = sync_flags;
    c17a[2] = 0;
    out_timings[0] = c16a[1] | ((short)c16a[0] << 8);
    out_timings[1] = c16a[3] | ((short)c16a[2] << 8);
    out_timings[2] = c16a[5] | ((short)c16a[4] << 8);
    out_timings[3] = c17a[7] | ((short)c17a[6] << 8);
    out_timings[4] = c17a[5] | ((short)c17a[4] << 8);
    out_timings[5] = c17a[3] | ((short)c17a[2] << 8);

    I830SDVOSetTargetInput(s);
    I830SDVOGetInputPixelClockRange(s, clock, height);

    I830SDVOGetActiveOutputs(s, 0);
    I830SDVOSetActiveOutputs(s, 0);

    I830SDVOSetTargetOutput(s);
    I830SDVOSetOutputTimingsPart1(s, clock, out_timings[0], out_timings[1],
				  out_timings[2]);

    I830SDVOSetTargetOutput(s);
    I830SDVOSetOutputTimingsPart2(s, out_timings[3], out_timings[4],
				  out_timings[5]);

    I830SDVOSetTargetInput(s);

    I830SDVOCreatePreferredInputTiming(s, clock, width, height);
    I830SDVOSetTargetInput(s);

    I830SDVOGetPreferredInputTimingPart1(s);
    I830SDVOSetTargetInput(s);

    I830SDVOGetPreferredInputTimingPart2(s, clock, out_timings[0], out_timings[1],
					 out_timings[2]);
    I830SDVOSetTargetInput(s);

    I830SDVOSetInputTimingsPart1(s, clock, curr_table[0], curr_table[1],
				 curr_table[2]);

    I830SDVOSetTargetInput(s);
    I830SDVOSetInputTimingsPart2(s, curr_table[3], curr_table[4],
				 out_timings[5]);

    I830SDVOSetTargetInput(s);
    /*if (mode->PrivFlags & I830_MFLAG_DOUBLE)
	I830SDVOSetClockRateMult(s, 0x02);
    else */
	I830SDVOSetClockRateMult(s, 0x01);

    return TRUE;
}

Bool
I830SDVOPostSetMode(I830SDVOPtr s, DisplayModePtr mode)
{
    int clock = mode->Clock/10, height=mode->CrtcVDisplay;
    Bool ret = TRUE;

    /* the BIOS writes out 6 commands post mode set */
    /* two 03s, 04 05, 10, 1d */
    /* these contain the height and mode clock / 10 by the looks of it */

    I830SDVOGetTrainedInputs(s, 1);
    I830SDVOGetTrainedInputs(s, 0);

    /* THIS IS A DIRTY HACK - sometimes for some reason on startup
     * the BIOS doesn't find my DVI monitor -
     * without this hack the driver doesn't work.. this causes the modesetting
     * to be re-run
     */
    if (s->sdvo_regs[SDVO_I2C_RETURN_0] != 0x1) {
	ret = FALSE;
    }

    I830SDVOGetActiveOutputs(s, 1);
    I830SDVOSetActiveOutputs(s, 1);

    I830SDVOSetTargetInput(s);
    I830SDVOGetInputPixelClockRange(s, clock, height);

    return ret;
}

I830SDVOPtr
I830SDVOInit(I2CBusPtr b)
{
    I830SDVOPtr sdvo;

    sdvo = xcalloc(1, sizeof(I830SDVORec));
    if (sdvo == NULL)
	return NULL;

    sdvo->d.DevName = "SDVO Controller";
    sdvo->d.SlaveAddr = 0x39 << 1;
    sdvo->d.pI2CBus = b;
    sdvo->d.StartTimeout = b->StartTimeout;
    sdvo->d.BitTimeout = b->BitTimeout;
    sdvo->d.AcknTimeout = b->AcknTimeout;
    sdvo->d.ByteTimeout = b->ByteTimeout;
    sdvo->d.DriverPrivate.ptr = sdvo;

    if (!xf86I2CDevInit(&sdvo->d)) {
	xf86DrvMsg(b->scrnIndex, X_ERROR,
		   "Failed to initialize SDVO I2C device\n");
	xfree(sdvo);
	return NULL;
    }
    return sdvo;
}

Bool
I830I2CDetectSDVOController(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned char ch[64];
    int i;
    I830SDVOPtr sdvo = pI830->output[output_index].sdvo_drv;

    if (sdvo == NULL)
	return FALSE;

    for (i = 0; i < 0x40; i++) {
	if (!sReadByte(sdvo, i, &ch[i]))
	    return FALSE;
    }

    pI830->output[output_index].sdvo_drv->found = 1;

    return TRUE;
}
