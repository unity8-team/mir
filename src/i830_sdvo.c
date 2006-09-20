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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "i830.h"
#include "i830_display.h"
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


#define SDVO_CMD_NAME_ENTRY(cmd) {cmd, #cmd}
const struct _sdvo_cmd_name {
    CARD8 cmd;
    char *name;
} sdvo_cmd_names[] = {
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_RESET),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_DEVICE_CAPS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FIRMWARE_REV),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TRAINED_INPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ATTACHED_DISPLAYS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HOT_PLUG_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INTR_EVENT_SOURCE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_INPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_OUTPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_CLOCK_RATE_MULTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_TV_FORMATS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_RESOLUTION_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CONTROL_BUS_SWITCH),
};
/* following on from tracing the intel BIOS i2c routines */
static void
I830SDVOWriteOutputs(I830SDVOPtr s, int num_out)
{
    int i;

    ErrorF("SDVO: W: %02X ", s->sdvo_regs[SDVO_I2C_OPCODE]);
    for (i = SDVO_I2C_ARG_0; i > SDVO_I2C_ARG_0 - num_out; i--)
	ErrorF("%02X ", s->sdvo_regs[i]);
    for (; i > SDVO_I2C_ARG_7; i--)
	ErrorF("   ");
    for (i = 0; i < sizeof(sdvo_cmd_names) / sizeof(sdvo_cmd_names[0]); i++) {
	if (s->sdvo_regs[SDVO_I2C_OPCODE] == sdvo_cmd_names[i].cmd) {
	    ErrorF("(%s)", sdvo_cmd_names[i].name);
	    break;
	}
    }
    ErrorF("\n");

    /* blast the output regs */
    for (i = SDVO_I2C_ARG_0; i > SDVO_I2C_ARG_0 - num_out; i--) {
	sWriteByte(s, i, s->sdvo_regs[i]);
    }
    /* blast the command reg */
    sWriteByte(s, SDVO_I2C_OPCODE, s->sdvo_regs[SDVO_I2C_OPCODE]);
}

static const char *cmd_status_names[] = {
	"Power on",
	"Success",
	"Not supported",
	"Invalid arg",
	"Pending",
	"Target not supported",
	"Scaling not supported"
};

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
    for (i = SDVO_I2C_RETURN_0; i <= SDVO_I2C_RETURN_7; i++)
	ErrorF("%02X ", s->sdvo_regs[i]);
    if (s->sdvo_regs[SDVO_I2C_CMD_STATUS] <= SDVO_CMD_STATUS_SCALING_NOT_SUPP)
	ErrorF("(%s)", cmd_status_names[s->sdvo_regs[SDVO_I2C_CMD_STATUS]]);
    else
	ErrorF("(??? %d)", s->sdvo_regs[SDVO_I2C_CMD_STATUS]);
    ErrorF("\n");
}

/* Sets the control bus switch to either point at one of the DDC buses or the
 * PROM.  It resets from the DDC bus back to internal registers at the next I2C
 * STOP.  PROM access is terminated by accessing an internal register.
 */
static Bool
I830SDVOSetControlBusSwitch(I830SDVOPtr s, CARD8 target)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CONTROL_BUS_SWITCH;
    s->sdvo_regs[SDVO_I2C_ARG_0] = target;

    I830SDVOWriteOutputs(s, 1);
    return TRUE;
}

static Bool
I830SDVOSetTargetInput(I830SDVOPtr s, Bool target_1, Bool target_2)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_INPUT;
    s->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
    s->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

    I830SDVOWriteOutputs(s, 2);

    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetTrainedInputs(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_TRAINED_INPUTS;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetActiveOutputs(I830SDVOPtr s, Bool *on_1, Bool *on_2)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ACTIVE_OUTPUTS;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    *on_1 = s->sdvo_regs[SDVO_I2C_RETURN_0];
    *on_2 = s->sdvo_regs[SDVO_I2C_RETURN_1];

    return TRUE;
}

static Bool
I830SDVOSetActiveOutputs(I830SDVOPtr s, Bool on_1, Bool on_2)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_ACTIVE_OUTPUTS;
    s->sdvo_regs[SDVO_I2C_ARG_0] = on_1;
    s->sdvo_regs[SDVO_I2C_ARG_1] = on_2;

    I830SDVOWriteOutputs(s, 2);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

static Bool
I830SDVOGetInputPixelClockRange(I830SDVOPtr s, CARD16 *clock_min,
				CARD16 *clock_max)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    *clock_min = s->sdvo_regs[SDVO_I2C_RETURN_0] |
		 (s->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    *clock_max = s->sdvo_regs[SDVO_I2C_RETURN_2] |
		 (s->sdvo_regs[SDVO_I2C_RETURN_3] << 8);

    return TRUE;
}

static Bool
I830SDVOSetTargetOutput(I830SDVOPtr s, Bool target_1, Bool target_2)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_OUTPUT;
    s->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
    s->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

    I830SDVOWriteOutputs(s, 2);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

/* Fetches either input or output timings to *dtd, depending on cmd. */
static Bool
I830SDVOGetTimings(I830SDVOPtr s, i830_sdvo_dtd *dtd, CARD8 cmd)
{
    memset(s->sdvo_regs, 0, 9);
    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    dtd->clock = s->sdvo_regs[SDVO_I2C_RETURN_0] |
		 (s->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    dtd->h_active = s->sdvo_regs[SDVO_I2C_RETURN_2];
    dtd->h_blank = s->sdvo_regs[SDVO_I2C_RETURN_3];
    dtd->h_high = s->sdvo_regs[SDVO_I2C_RETURN_4];
    dtd->v_active = s->sdvo_regs[SDVO_I2C_RETURN_5];
    dtd->v_blank = s->sdvo_regs[SDVO_I2C_RETURN_6];
    dtd->v_high = s->sdvo_regs[SDVO_I2C_RETURN_7];

    memset(s->sdvo_regs, 0, 9);
    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd + 1;
    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    dtd->h_sync_off = s->sdvo_regs[SDVO_I2C_RETURN_0];
    dtd->h_sync_width = s->sdvo_regs[SDVO_I2C_RETURN_1];
    dtd->v_sync_off_width = s->sdvo_regs[SDVO_I2C_RETURN_2];
    dtd->sync_off_width_high = s->sdvo_regs[SDVO_I2C_RETURN_3];
    dtd->dtd_flags = s->sdvo_regs[SDVO_I2C_RETURN_4];
    dtd->sdvo_flags = s->sdvo_regs[SDVO_I2C_RETURN_5];
    dtd->v_sync_off_high = s->sdvo_regs[SDVO_I2C_RETURN_6];
    dtd->reserved = s->sdvo_regs[SDVO_I2C_RETURN_7];

    return TRUE;
}

/* Sets either input or output timings to *dtd, depending on cmd. */
static Bool
I830SDVOSetTimings(I830SDVOPtr s, i830_sdvo_dtd *dtd, CARD8 cmd)
{
    memset(s->sdvo_regs, 0, 9);
    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
    s->sdvo_regs[SDVO_I2C_ARG_0] = dtd->clock & 0xff;
    s->sdvo_regs[SDVO_I2C_ARG_1] = dtd->clock >> 8;
    s->sdvo_regs[SDVO_I2C_ARG_2] = dtd->h_active;
    s->sdvo_regs[SDVO_I2C_ARG_3] = dtd->h_blank;
    s->sdvo_regs[SDVO_I2C_ARG_4] = dtd->h_high;
    s->sdvo_regs[SDVO_I2C_ARG_5] = dtd->v_active;
    s->sdvo_regs[SDVO_I2C_ARG_6] = dtd->v_blank;
    s->sdvo_regs[SDVO_I2C_ARG_7] = dtd->v_high;
    I830SDVOWriteOutputs(s, 8);
    I830SDVOReadInputRegs(s);

    memset(s->sdvo_regs, 0, 9);
    s->sdvo_regs[SDVO_I2C_OPCODE] = cmd + 1;
    s->sdvo_regs[SDVO_I2C_ARG_0] = dtd->h_sync_off;
    s->sdvo_regs[SDVO_I2C_ARG_1] = dtd->h_sync_width;
    s->sdvo_regs[SDVO_I2C_ARG_2] = dtd->v_sync_off_width;
    s->sdvo_regs[SDVO_I2C_ARG_3] = dtd->sync_off_width_high;
    s->sdvo_regs[SDVO_I2C_ARG_4] = dtd->dtd_flags;
    s->sdvo_regs[SDVO_I2C_ARG_5] = dtd->sdvo_flags;
    s->sdvo_regs[SDVO_I2C_ARG_6] = dtd->v_sync_off_high;
    s->sdvo_regs[SDVO_I2C_ARG_7] = dtd->reserved;
    I830SDVOWriteOutputs(s, 7);
    I830SDVOReadInputRegs(s);

    return TRUE;
}

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

    I830SDVOWriteOutputs(s, 8);
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

    I830SDVOWriteOutputs(s, 8);
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
    s->sdvo_regs[SDVO_I2C_ARG_4] = height & 0xff;  
    s->sdvo_regs[SDVO_I2C_ARG_5] = (height >> 8) & 0xff;

    I830SDVOWriteOutputs(s, 7);
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
I830SDVOGetPreferredInputTimingPart2(I830SDVOPtr s)
{
    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    curr_table[3] = s->sdvo_regs[SDVO_I2C_RETURN_0] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    curr_table[4] = s->sdvo_regs[SDVO_I2C_RETURN_2] |
		    (s->sdvo_regs[SDVO_I2C_RETURN_3] << 8);
    curr_table[5] = 0x1e;

    return TRUE;
}

static int
I830SDVOGetClockRateMult(I830SDVOPtr s)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_CLOCK_RATE_MULT;

    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    if (s->sdvo_regs[SDVO_I2C_CMD_STATUS] != SDVO_CMD_STATUS_SUCCESS) {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR,
		   "Couldn't get SDVO clock rate multiplier\n");
	return SDVO_CLOCK_RATE_MULT_1X;
    } else {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_INFO,
		   "Current clock rate multiplier: %d\n",
		   s->sdvo_regs[SDVO_I2C_RETURN_0]);
    }

    return s->sdvo_regs[SDVO_I2C_RETURN_0];
}

static Bool
I830SDVOSetClockRateMult(I830SDVOPtr s, CARD8 val)
{
    memset(s->sdvo_regs, 0, 9);

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CLOCK_RATE_MULT;

    s->sdvo_regs[SDVO_I2C_ARG_0] = val;
    I830SDVOWriteOutputs(s, 1);
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
    CARD16 clock_min, clock_max;
    Bool out1, out2;

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

    I830SDVOSetTargetInput(s, FALSE, FALSE);
    I830SDVOGetInputPixelClockRange(s, &clock_min, &clock_max);
    ErrorF("clock min/max: %d %d\n", clock_min, clock_max);

    I830SDVOGetActiveOutputs(s, &out1, &out2);
    
    I830SDVOSetActiveOutputs(s, FALSE, FALSE);

    I830SDVOSetTargetOutput(s, TRUE, FALSE);
    I830SDVOSetOutputTimingsPart1(s, clock, out_timings[0], out_timings[1],
				  out_timings[2]);
    I830SDVOSetOutputTimingsPart2(s, out_timings[3], out_timings[4],
				  out_timings[5]);

    I830SDVOSetTargetInput (s, FALSE, FALSE);
    
    I830SDVOCreatePreferredInputTiming(s, clock, width, height);
    I830SDVOGetPreferredInputTimingPart1(s);
    I830SDVOGetPreferredInputTimingPart2(s);
    
    I830SDVOSetTargetInput (s, FALSE, FALSE);
    
    I830SDVOSetInputTimingsPart1(s, clock, curr_table[0], curr_table[1],
				 curr_table[2]);
    I830SDVOSetInputTimingsPart2(s, curr_table[3], curr_table[4],
				 out_timings[5]);

    I830SDVOSetTargetInput (s, FALSE, FALSE);
    
    if (clock >= 10000)
	I830SDVOSetClockRateMult(s, SDVO_CLOCK_RATE_MULT_1X);
    else if (clock >= 5000)
	I830SDVOSetClockRateMult(s, SDVO_CLOCK_RATE_MULT_2X);
    else
	I830SDVOSetClockRateMult(s, SDVO_CLOCK_RATE_MULT_4X);

    return TRUE;
}

Bool
I830SDVOPostSetMode(I830SDVOPtr s, DisplayModePtr mode)
{
    Bool ret = TRUE;
    Bool out1, out2;

    /* the BIOS writes out 6 commands post mode set */
    /* two 03s, 04 05, 10, 1d */
    /* these contain the height and mode clock / 10 by the looks of it */

    I830SDVOGetTrainedInputs(s);

    /* THIS IS A DIRTY HACK - sometimes for some reason on startup
     * the BIOS doesn't find my DVI monitor -
     * without this hack the driver doesn't work.. this causes the modesetting
     * to be re-run
     */
    if (s->sdvo_regs[SDVO_I2C_RETURN_0] != 0x1) {
	ret = FALSE;
    }

    I830SDVOGetActiveOutputs (s, &out1, &out2);
    I830SDVOSetActiveOutputs(s, TRUE, FALSE);
    I830SDVOSetTargetInput (s, FALSE, FALSE);

    return ret;
}

void
i830SDVOSave(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr sdvo = pI830->output[output_index].sdvo_drv;

    sdvo->save_sdvo_mult = I830SDVOGetClockRateMult(sdvo);
    I830SDVOGetActiveOutputs(sdvo, &sdvo->save_sdvo_active_1,
			     &sdvo->save_sdvo_active_2);

    if (sdvo->caps.caps & 0x1) {
       I830SDVOSetTargetInput(sdvo, FALSE, FALSE);
       I830SDVOGetTimings(sdvo, &sdvo->save_input_dtd_1,
			  SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    }

    if (sdvo->caps.caps & 0x2) {
       I830SDVOSetTargetInput(sdvo, FALSE, TRUE);
       I830SDVOGetTimings(sdvo, &sdvo->save_input_dtd_2,
			  SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    }
    
    if (sdvo->caps.output_0_supported) {
       I830SDVOSetTargetOutput(sdvo, TRUE, FALSE);
       I830SDVOGetTimings(sdvo, &sdvo->save_output_dtd_1,
			  SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    }

    if (sdvo->caps.output_1_supported) {
       I830SDVOSetTargetOutput(sdvo, FALSE, TRUE);
       I830SDVOGetTimings(sdvo, &sdvo->save_output_dtd_2,
			  SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    }

    sdvo->save_SDVOX = INREG(sdvo->output_device);
}

void
i830SDVOPreRestore(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr sdvo = pI830->output[output_index].sdvo_drv;

    I830SDVOSetActiveOutputs(sdvo, FALSE, FALSE);
}

void
i830SDVOPostRestore(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr sdvo = pI830->output[output_index].sdvo_drv;

    if (sdvo->caps.caps & 0x1) {
       I830SDVOSetTargetInput(sdvo, FALSE, FALSE);
       I830SDVOSetTimings(sdvo, &sdvo->save_input_dtd_1,
			  SDVO_CMD_SET_INPUT_TIMINGS_PART1);
    }

    if (sdvo->caps.caps & 0x2) {
       I830SDVOSetTargetInput(sdvo, FALSE, TRUE);
       I830SDVOSetTimings(sdvo, &sdvo->save_input_dtd_2,
			  SDVO_CMD_SET_INPUT_TIMINGS_PART1);
    }

    if (sdvo->caps.output_0_supported) {
       I830SDVOSetTargetOutput(sdvo, TRUE, FALSE);
       I830SDVOSetTimings(sdvo, &sdvo->save_output_dtd_1,
			  SDVO_CMD_SET_OUTPUT_TIMINGS_PART1);
    }

    if (sdvo->caps.output_1_supported) {
       I830SDVOSetTargetOutput(sdvo, FALSE, TRUE);
       I830SDVOSetTimings(sdvo, &sdvo->save_output_dtd_2,
			  SDVO_CMD_SET_OUTPUT_TIMINGS_PART1);
    }

    I830SDVOSetClockRateMult(sdvo, sdvo->save_sdvo_mult);

    OUTREG(sdvo->output_device, sdvo->save_SDVOX);

    I830SDVOSetActiveOutputs(sdvo, sdvo->save_sdvo_active_1,
			     sdvo->save_sdvo_active_2);
}

static void
I830SDVOGetCapabilities(I830SDVOPtr s, i830_sdvo_caps *caps)
{
  memset(s->sdvo_regs, 0, 9);
  s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_DEVICE_CAPS;
  I830SDVOWriteOutputs(s, 0);
  I830SDVOReadInputRegs(s);

  caps->vendor_id = s->sdvo_regs[SDVO_I2C_RETURN_0];
  caps->device_id = s->sdvo_regs[SDVO_I2C_RETURN_1];
  caps->device_rev_id = s->sdvo_regs[SDVO_I2C_RETURN_2];
  caps->sdvo_version_major = s->sdvo_regs[SDVO_I2C_RETURN_3];
  caps->sdvo_version_minor = s->sdvo_regs[SDVO_I2C_RETURN_4];
  caps->caps = s->sdvo_regs[SDVO_I2C_RETURN_5];
  caps->output_0_supported = s->sdvo_regs[SDVO_I2C_RETURN_6];
  caps->output_1_supported = s->sdvo_regs[SDVO_I2C_RETURN_7];
}

static Bool
I830SDVODDCI2CGetByte(I2CDevPtr d, I2CByte *data, Bool last)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CGetByte(d, data, last);
    d->pI2CBus = savebus;

    return ret;
}

static Bool
I830SDVODDCI2CPutByte(I2CDevPtr d, I2CByte c)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CPutByte(d, c);
    d->pI2CBus = savebus;

    return ret;
}

static Bool
I830SDVODDCI2CStart(I2CBusPtr b, int timeout)
{
    I830SDVOPtr sdvo = b->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus;

    I830SDVOSetControlBusSwitch(sdvo, SDVO_CONTROL_BUS_DDC2);
    return i2cbus->I2CStart(i2cbus, timeout);
}

static void
I830SDVODDCI2CStop(I2CDevPtr d)
{
    I830SDVOPtr sdvo = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = sdvo->d.pI2CBus, savebus;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    i2cbus->I2CStop(d);
    d->pI2CBus = savebus;
}

/* It's a shame that xf86i2c.c's I2CAddress() doesn't use the bus's pointers,
 * so it's useless to us here.
 */
static Bool
I830SDVODDCI2CAddress(I2CDevPtr d, I2CSlaveAddr addr)
{
    if (d->pI2CBus->I2CStart(d->pI2CBus, d->StartTimeout)) {
	if (d->pI2CBus->I2CPutByte(d, addr & 0xFF)) {
	    if ((addr & 0xF8) != 0xF0 &&
		(addr & 0xFE) != 0x00)
		return TRUE;

	    if (d->pI2CBus->I2CPutByte(d, (addr >> 8) & 0xFF))
		return TRUE;
	}

	d->pI2CBus->I2CStop(d);
    }

    return FALSE;
}

I830SDVOPtr
I830SDVOInit(ScrnInfoPtr pScrn, int output_index, CARD32 output_device)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr sdvo;
    int i;
    unsigned char ch[0x40];
    I2CBusPtr i2cbus, ddcbus;

    i2cbus = pI830->output[output_index].pI2CBus;

    sdvo = xcalloc(1, sizeof(I830SDVORec));
    if (sdvo == NULL)
	return NULL;

    if (output_device == SDVOB) {
	sdvo->d.DevName = "SDVO Controller B";
	sdvo->d.SlaveAddr = 0x70;
    } else {
	sdvo->d.DevName = "SDVO Controller C";
	sdvo->d.SlaveAddr = 0x72;
    }
    sdvo->d.pI2CBus = i2cbus;
    sdvo->d.DriverPrivate.ptr = sdvo;
    sdvo->output_device = output_device;

    if (!xf86I2CDevInit(&sdvo->d)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Failed to initialize SDVO I2C device %s\n",
		   output_device == SDVOB ? "SDVOB" : "SDVOC");
	xfree(sdvo);
	return NULL;
    }

    /* Set up our wrapper I2C bus for DDC.  It acts just like the regular I2C
     * bus, except that it does the control bus switch to DDC mode before every
     * Start.  While we only need to do it at Start after every Stop after a
     * Start, extra attempts should be harmless.
     */
    ddcbus = xf86CreateI2CBusRec();
    if (ddcbus == NULL) {
	xf86DestroyI2CDevRec(&sdvo->d, 0);
	xfree(sdvo);
	return NULL;
    }
    if (output_device == SDVOB)
        ddcbus->BusName = "SDVOB DDC Bus";
    else
        ddcbus->BusName = "SDVOC DDC Bus";
    ddcbus->scrnIndex = i2cbus->scrnIndex;
    ddcbus->I2CGetByte = I830SDVODDCI2CGetByte;
    ddcbus->I2CPutByte = I830SDVODDCI2CPutByte;
    ddcbus->I2CStart = I830SDVODDCI2CStart;
    ddcbus->I2CStop = I830SDVODDCI2CStop;
    ddcbus->I2CAddress = I830SDVODDCI2CAddress;
    ddcbus->DriverPrivate.ptr = sdvo;
    if (!xf86I2CBusInit(ddcbus)) {
	xf86DestroyI2CDevRec(&sdvo->d, 0);
	xfree(sdvo);
	return NULL;
    }

    pI830->output[output_index].pDDCBus = ddcbus;

    /* Read the regs to test if we can talk to the device */
    for (i = 0; i < 0x40; i++) {
	if (!sReadByte(sdvo, i, &ch[i])) {
	    xf86DestroyI2CBusRec(pI830->output[output_index].pDDCBus, FALSE,
		FALSE);
	    xf86DestroyI2CDevRec(&sdvo->d, 0);
	    xfree(sdvo);
	    return NULL;
	}
    }

    pI830->output[output_index].sdvo_drv = sdvo;

    I830SDVOGetCapabilities(sdvo, &sdvo->caps);
    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "SDVO device VID/DID: %02X:%02X.%02X, %02X, output 1: %c, output 2: %c\n",
	       sdvo->caps.vendor_id, sdvo->caps.device_id,
	       sdvo->caps.device_rev_id, sdvo->caps.caps,
	       sdvo->caps.output_0_supported ? 'Y' : 'N',
	       sdvo->caps.output_1_supported ? 'Y' : 'N');

    return sdvo;
}

static void
I830DumpSDVOCmd (I830SDVOPtr s, int opcode)
{
    memset (s->sdvo_regs, 0, sizeof (s->sdvo_regs));
    s->sdvo_regs[SDVO_I2C_OPCODE] = opcode;
    I830SDVOWriteOutputs (s, 0);
    I830SDVOReadInputRegs (s);
}

static void
I830DumpOneSDVO (I830SDVOPtr s)
{
    ErrorF ("Dump %s\n", s->d.DevName);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_DEVICE_CAPS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_FIRMWARE_REV);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_TRAINED_INPUTS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_ACTIVE_OUTPUTS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_IN_OUT_MAP);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_ATTACHED_DISPLAYS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_HOT_PLUG_SUPPORT);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_ACTIVE_HOT_PLUG);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_INTR_EVENT_SOURCE);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_INPUT_TIMINGS_PART2);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_OUTPUT_TIMINGS_PART2);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_OUTPUT_PIXEL_CLOCK_RANGE);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_SUPPORTED_CLOCK_RATE_MULTS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_CLOCK_RATE_MULT);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_SUPPORTED_TV_FORMATS);
    I830DumpSDVOCmd (s, SDVO_CMD_GET_TV_FORMAT);
}
		 
void
I830DumpSDVO (ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr	s;
    int	i;

    for (i = 0; i < 4; i++) {
	s = pI830->output[i].sdvo_drv;
	if (s)
	    I830DumpOneSDVO (s);
    }
}

/**
 * Asks the SDVO device if any displays are currently connected.
 *
 * This interface will need to be augmented, since we could potentially have
 * multiple displays connected, and the caller will also probably want to know
 * what type of display is connected.  But this is enough for the moment.
 *
 * Takes 14ms on average on my i945G.
 */
Bool
I830DetectSDVODisplays(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830SDVOPtr s = pI830->output[output_index].sdvo_drv;

    s->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ATTACHED_DISPLAYS;
    I830SDVOWriteOutputs(s, 0);
    I830SDVOReadInputRegs(s);

    if (s->sdvo_regs[SDVO_I2C_CMD_STATUS] != SDVO_CMD_STATUS_SUCCESS)
	return FALSE;

    return (s->sdvo_regs[SDVO_I2C_RETURN_0] != 0 ||
	    s->sdvo_regs[SDVO_I2C_RETURN_1] != 0);
}
