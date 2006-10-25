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

/** @file
 * SDVO support for i915 and newer chipsets.
 *
 * The SDVO outputs send digital display data out over the PCIE bus to display
 * cards implementing a defined interface.  These cards may have DVI, TV, CRT,
 * or other outputs on them.
 *
 * The system has two SDVO channels, which may be used for SDVO chips on the
 * motherboard, or in the external cards.  The two channels may also be used
 * in a ganged mode to provide higher bandwidth to a single output.  Currently,
 * this code doesn't deal with either ganged mode or more than one SDVO output.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "i830.h"
#include "i830_display.h"
#include "i810_reg.h"
#include "i830_sdvo_regs.h"

/** SDVO driver private structure. */
struct i830_sdvo_priv {
    /** SDVO device on SDVO I2C bus. */
    I2CDevRec d;
    /** Temporary storage for reg read/writes */
    unsigned char sdvo_regs[20];
    /** Register for the SDVO device: SDVOB or SDVOC */
    int output_device;
    /**
     * Capabilities of the SDVO device returned by i830_sdvo_get_capabilities()
     */
    i830_sdvo_caps caps;
    /** Pixel clock limitations reported by the SDVO device */
    CARD16 pixel_clock_min, pixel_clock_max;

    /** State for save/restore */
    /** @{ */
    int save_sdvo_mult;
    Bool save_sdvo_active_1, save_sdvo_active_2;
    i830_sdvo_dtd save_input_dtd_1, save_input_dtd_2;
    i830_sdvo_dtd save_output_dtd_1, save_output_dtd_2;
    CARD32 save_SDVOX;
    /** @} */
};

CARD16 curr_table[6];

/** Read a single byte from the given address on the SDVO device. */
static Bool i830_sdvo_read_byte(I830OutputPtr output, int addr,
				unsigned char *ch)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    if (!xf86I2CReadByte(&dev_priv->d, addr, ch)) {
	xf86DrvMsg(output->pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read from %s slave %d.\n",
		   output->pI2CBus->BusName, dev_priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/** Write a single byte to the given address on the SDVO device. */
static Bool i830_sdvo_write_byte(I830OutputPtr output,
				 int addr, unsigned char ch)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    if (!xf86I2CWriteByte(&dev_priv->d, addr, ch)) {
	xf86DrvMsg(output->pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n",
		   output->pI2CBus->BusName, dev_priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}


#define SDVO_CMD_NAME_ENTRY(cmd) {cmd, #cmd}
/** Mapping of command numbers to names, for debug output */
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
i830_sdvo_write_outputs(I830OutputPtr output, int num_out)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;
    int i;

    ErrorF("SDVO: W: %02X ", dev_priv->sdvo_regs[SDVO_I2C_OPCODE]);
    for (i = SDVO_I2C_ARG_0; i > SDVO_I2C_ARG_0 - num_out; i--)
	ErrorF("%02X ", dev_priv->sdvo_regs[i]);
    for (; i > SDVO_I2C_ARG_7; i--)
	ErrorF("   ");
    for (i = 0; i < sizeof(sdvo_cmd_names) / sizeof(sdvo_cmd_names[0]); i++) {
	if (dev_priv->sdvo_regs[SDVO_I2C_OPCODE] == sdvo_cmd_names[i].cmd) {
	    ErrorF("(%s)", sdvo_cmd_names[i].name);
	    break;
	}
    }
    ErrorF("\n");

    /* blast the output regs */
    for (i = SDVO_I2C_ARG_0; i > SDVO_I2C_ARG_0 - num_out; i--) {
	i830_sdvo_write_byte(output, i, dev_priv->sdvo_regs[i]);
    }
    /* blast the command reg */
    i830_sdvo_write_byte(output, SDVO_I2C_OPCODE,
			 dev_priv->sdvo_regs[SDVO_I2C_OPCODE]);
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
i830_sdvo_read_input_regs(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;
    int i;

    /* follow BIOS ordering */
    i830_sdvo_read_byte(output, SDVO_I2C_CMD_STATUS,
			&dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS]);

    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_3,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_3]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_2,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_2]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_1,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_1]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_0,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_0]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_7,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_7]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_6,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_6]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_5,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_5]);
    i830_sdvo_read_byte(output, SDVO_I2C_RETURN_4,
			&dev_priv->sdvo_regs[SDVO_I2C_RETURN_4]);

    ErrorF("SDVO: R: ");
    for (i = SDVO_I2C_RETURN_0; i <= SDVO_I2C_RETURN_7; i++)
	ErrorF("%02X ", dev_priv->sdvo_regs[i]);
    if (dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS] <=
	SDVO_CMD_STATUS_SCALING_NOT_SUPP)
    {
	ErrorF("(%s)",
	       cmd_status_names[dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS]]);
    } else {
	ErrorF("(??? %d)", dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS]);
    }
    ErrorF("\n");
}

int
i830_sdvo_get_pixel_multiplier(DisplayModePtr pMode)
{
    if (pMode->Clock >= 100000)
	return 1;
    else if (pMode->Clock >= 50000)
	return 2;
    else
	return 4;
}

/* Sets the control bus switch to either point at one of the DDC buses or the
 * PROM.  It resets from the DDC bus back to internal registers at the next I2C
 * STOP.  PROM access is terminated by accessing an internal register.
 */
static Bool
i830_sdvo_set_control_bus_switch(I830OutputPtr output, CARD8 target)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CONTROL_BUS_SWITCH;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = target;

    i830_sdvo_write_outputs(output, 1);
    return TRUE;
}

static Bool
i830_sdvo_set_target_input(I830OutputPtr output, Bool target_1, Bool target_2)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_INPUT;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

    i830_sdvo_write_outputs(output, 2);

    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_get_trained_inputs(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_TRAINED_INPUTS;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_get_active_outputs(I830OutputPtr output, Bool *on_1, Bool *on_2)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ACTIVE_OUTPUTS;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    *on_1 = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0];
    *on_2 = dev_priv->sdvo_regs[SDVO_I2C_RETURN_1];

    return TRUE;
}

static Bool
i830_sdvo_set_active_outputs(I830OutputPtr output, Bool on_1, Bool on_2)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_ACTIVE_OUTPUTS;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = on_1;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = on_2;

    i830_sdvo_write_outputs(output, 2);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_get_input_pixel_clock_range(I830OutputPtr output, CARD16 *clock_min,
				      CARD16 *clock_max)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] =
	SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    *clock_min = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0] |
		 (dev_priv->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    *clock_max = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2] |
		 (dev_priv->sdvo_regs[SDVO_I2C_RETURN_3] << 8);

    return TRUE;
}

static Bool
i830_sdvo_set_target_output(I830OutputPtr output, Bool target_1, Bool target_2)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_TARGET_OUTPUT;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = target_1;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = target_2;

    i830_sdvo_write_outputs(output, 2);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

/* Fetches either input or output timings to *dtd, depending on cmd. */
static Bool
i830_sdvo_get_timings(I830OutputPtr output, i830_sdvo_dtd *dtd, CARD8 cmd)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    dtd->clock = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0] |
		 (dev_priv->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    dtd->h_active = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2];
    dtd->h_blank = dev_priv->sdvo_regs[SDVO_I2C_RETURN_3];
    dtd->h_high = dev_priv->sdvo_regs[SDVO_I2C_RETURN_4];
    dtd->v_active = dev_priv->sdvo_regs[SDVO_I2C_RETURN_5];
    dtd->v_blank = dev_priv->sdvo_regs[SDVO_I2C_RETURN_6];
    dtd->v_high = dev_priv->sdvo_regs[SDVO_I2C_RETURN_7];

    memset(dev_priv->sdvo_regs, 0, 9);
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd + 1;
    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    dtd->h_sync_off = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0];
    dtd->h_sync_width = dev_priv->sdvo_regs[SDVO_I2C_RETURN_1];
    dtd->v_sync_off_width = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2];
    dtd->sync_off_width_high = dev_priv->sdvo_regs[SDVO_I2C_RETURN_3];
    dtd->dtd_flags = dev_priv->sdvo_regs[SDVO_I2C_RETURN_4];
    dtd->sdvo_flags = dev_priv->sdvo_regs[SDVO_I2C_RETURN_5];
    dtd->v_sync_off_high = dev_priv->sdvo_regs[SDVO_I2C_RETURN_6];
    dtd->reserved = dev_priv->sdvo_regs[SDVO_I2C_RETURN_7];

    return TRUE;
}

/* Sets either input or output timings to *dtd, depending on cmd. */
static Bool
i830_sdvo_set_timings(I830OutputPtr output, i830_sdvo_dtd *dtd, CARD8 cmd)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = dtd->clock & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = dtd->clock >> 8;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_2] = dtd->h_active;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_3] = dtd->h_blank;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_4] = dtd->h_high;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_5] = dtd->v_active;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_6] = dtd->v_blank;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_7] = dtd->v_high;
    i830_sdvo_write_outputs(output, 8);
    i830_sdvo_read_input_regs(output);

    memset(dev_priv->sdvo_regs, 0, 9);
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd + 1;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = dtd->h_sync_off;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = dtd->h_sync_width;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_2] = dtd->v_sync_off_width;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_3] = dtd->sync_off_width_high;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_4] = dtd->dtd_flags;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_5] = dtd->sdvo_flags;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_6] = dtd->v_sync_off_high;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_7] = dtd->reserved;
    i830_sdvo_write_outputs(output, 7);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_set_timings_part1(I830OutputPtr output, char cmd, CARD16 clock,
			    CARD16 magic1, CARD16 magic2, CARD16 magic3)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd;

    /* set clock regs */
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_2] = magic3 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_3] = (magic3 >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_4] = magic2 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_5] = (magic2 >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_6] = magic1 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_7] = (magic1 >> 8) & 0xff;

    i830_sdvo_write_outputs(output, 8);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_set_input_timings_part1(I830OutputPtr output, CARD16 clock,
				  CARD16 magic1, CARD16 magic2, CARD16 magic3)
{
    return i830_sdvo_set_timings_part1(output,
				       SDVO_CMD_SET_INPUT_TIMINGS_PART1,
				       clock, magic1, magic2, magic3);
}

static Bool
i830_sdvo_set_output_timings_part1(I830OutputPtr output, CARD16 clock,
				   CARD16 magic1, CARD16 magic2, CARD16 magic3)
{
    return i830_sdvo_set_timings_part1(output,
				       SDVO_CMD_SET_OUTPUT_TIMINGS_PART1,
				       clock, magic1, magic2, magic3);
}

static Bool
i830_sdvo_set_timings_part2(I830OutputPtr output, CARD8 cmd,
			    CARD16 magic4, CARD16 magic5, CARD16 magic6)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = cmd;

    /* set clock regs */
    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = magic4 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = (magic4 >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_2] = magic5 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_3] = (magic5 >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_4] = magic6 & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_5] = (magic6 >> 8) & 0xff;

    i830_sdvo_write_outputs(output, 8);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_set_input_timings_part2(I830OutputPtr output,
				  CARD16 magic4, CARD16 magic5, CARD16 magic6)
{
    return i830_sdvo_set_timings_part2(output,
				       SDVO_CMD_SET_INPUT_TIMINGS_PART2,
				       magic4, magic5, magic6);
}

static Bool
i830_sdvo_set_output_timings_part2(I830OutputPtr output,
			      CARD16 magic4, CARD16 magic5, CARD16 magic6)
{
    return i830_sdvo_set_timings_part2(output,
				       SDVO_CMD_SET_OUTPUT_TIMINGS_PART2,
				       magic4, magic5, magic6);
}

static Bool
i830_sdvo_create_preferred_input_timing(I830OutputPtr output, CARD16 clock,
					CARD16 width, CARD16 height)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] =
	SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING;

    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = clock & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_1] = (clock >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_2] = width & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_3] = (width >> 8) & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_4] = height & 0xff;
    dev_priv->sdvo_regs[SDVO_I2C_ARG_5] = (height >> 8) & 0xff;

    i830_sdvo_write_outputs(output, 7);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static Bool
i830_sdvo_get_preferred_input_timing_part1(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] =
	SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    curr_table[0] = dev_priv->sdvo_regs[SDVO_I2C_RETURN_6] |
		    (dev_priv->sdvo_regs[SDVO_I2C_RETURN_7] << 8);
    curr_table[1] = dev_priv->sdvo_regs[SDVO_I2C_RETURN_4] |
		    (dev_priv->sdvo_regs[SDVO_I2C_RETURN_5] << 8);
    curr_table[2] = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2] |
		    (dev_priv->sdvo_regs[SDVO_I2C_RETURN_3] << 8);

    return TRUE;
}

static Bool
i830_sdvo_get_preferred_input_timing_part2(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] =
	SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    curr_table[3] = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0] |
		    (dev_priv->sdvo_regs[SDVO_I2C_RETURN_1] << 8);
    curr_table[4] = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2] |
		    (dev_priv->sdvo_regs[SDVO_I2C_RETURN_3] << 8);
    curr_table[5] = 0x1e;

    return TRUE;
}

static int
i830_sdvo_get_clock_rate_mult(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_CLOCK_RATE_MULT;

    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    if (dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS] != SDVO_CMD_STATUS_SUCCESS) {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Couldn't get SDVO clock rate multiplier\n");
	return SDVO_CLOCK_RATE_MULT_1X;
    } else {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_INFO,
		   "Current clock rate multiplier: %d\n",
		   dev_priv->sdvo_regs[SDVO_I2C_RETURN_0]);
    }

    return dev_priv->sdvo_regs[SDVO_I2C_RETURN_0];
}

static Bool
i830_sdvo_set_clock_rate_mult(I830OutputPtr output, CARD8 val)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_SET_CLOCK_RATE_MULT;

    dev_priv->sdvo_regs[SDVO_I2C_ARG_0] = val;
    i830_sdvo_write_outputs(output, 1);
    i830_sdvo_read_input_regs(output);

    return TRUE;
}

static void
i830_sdvo_pre_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		       DisplayModePtr mode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD16 clock = mode->Clock/10, width = mode->CrtcHDisplay;
    CARD16 height = mode->CrtcVDisplay;
    CARD16 h_blank_len, h_sync_len, v_blank_len, v_sync_len;
    CARD16 h_sync_offset, v_sync_offset;
    CARD16 sync_flags;
    CARD8 c16a[8];
    CARD8 c17a[8];
    CARD16 out_timings[6];
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

    i830_sdvo_set_target_input(output, FALSE, FALSE);

    i830_sdvo_get_active_outputs(output, &out1, &out2);

    i830_sdvo_set_active_outputs(output, FALSE, FALSE);

    i830_sdvo_set_target_output(output, TRUE, FALSE);
    i830_sdvo_set_output_timings_part1(output, clock,
				       out_timings[0], out_timings[1],
				       out_timings[2]);
    i830_sdvo_set_output_timings_part2(output, out_timings[3], out_timings[4],
				       out_timings[5]);

    i830_sdvo_set_target_input(output, FALSE, FALSE);

    i830_sdvo_create_preferred_input_timing(output, clock, width, height);
    i830_sdvo_get_preferred_input_timing_part1(output);
    i830_sdvo_get_preferred_input_timing_part2(output);

    i830_sdvo_set_target_input(output, FALSE, FALSE);

    i830_sdvo_set_input_timings_part1(output, clock,
				      curr_table[0], curr_table[1],
				      curr_table[2]);
    i830_sdvo_set_input_timings_part2(output, curr_table[3], curr_table[4],
				      out_timings[5]);

    i830_sdvo_set_target_input(output, FALSE, FALSE);

    switch (i830_sdvo_get_pixel_multiplier(mode)) {
    case 1:
	i830_sdvo_set_clock_rate_mult(output, SDVO_CLOCK_RATE_MULT_1X);
	break;
    case 2:
	i830_sdvo_set_clock_rate_mult(output, SDVO_CLOCK_RATE_MULT_2X);
	break;
    case 4:
	i830_sdvo_set_clock_rate_mult(output, SDVO_CLOCK_RATE_MULT_4X);
	break;
    }

    OUTREG(SDVOC, INREG(SDVOC) & ~SDVO_ENABLE);
    OUTREG(SDVOB, INREG(SDVOB) & ~SDVO_ENABLE);
}

static void
i830_sdvo_post_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
			DisplayModePtr mode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct i830_sdvo_priv *dev_priv = output->dev_priv;
    Bool ret = TRUE;
    Bool out1, out2;
    CARD32 dpll, sdvob, sdvoc;
    int dpll_reg = (output->pipe == 0) ? DPLL_A : DPLL_B;
    int sdvo_pixel_multiply;

    /* the BIOS writes out 6 commands post mode set */
    /* two 03s, 04 05, 10, 1d */
    /* these contain the height and mode clock / 10 by the looks of it */

    i830_sdvo_get_trained_inputs(output);

    /* THIS IS A DIRTY HACK - sometimes for some reason on startup
     * the BIOS doesn't find my DVI monitor -
     * without this hack the driver doesn't work.. this causes the modesetting
     * to be re-run
     */
    if (dev_priv->sdvo_regs[SDVO_I2C_RETURN_0] != 0x1) {
	ret = FALSE;
    }

    i830_sdvo_get_active_outputs(output, &out1, &out2);
    i830_sdvo_set_active_outputs(output, TRUE, FALSE);
    i830_sdvo_set_target_input(output, FALSE, FALSE);

    /* Set the SDVO control regs. */
    sdvob = INREG(SDVOB) & SDVOB_PRESERVE_MASK;
    sdvoc = INREG(SDVOC) & SDVOC_PRESERVE_MASK;
    sdvob |= SDVO_ENABLE | (9 << 19) | SDVO_BORDER_ENABLE;
    sdvoc |= 9 << 19;
    if (output->pipe == 1)
	sdvob |= SDVO_PIPE_B_SELECT;

    dpll = INREG(dpll_reg);

    sdvo_pixel_multiply = i830_sdvo_get_pixel_multiplier(mode);
    if (IS_I945G(pI830) || IS_I945GM(pI830))
	dpll |= (sdvo_pixel_multiply - 1) << SDVO_MULTIPLIER_SHIFT_HIRES;
    else
	sdvob |= (sdvo_pixel_multiply - 1) << SDVO_PORT_MULTIPLY_SHIFT;

    OUTREG(dpll_reg, dpll | DPLL_DVO_HIGH_SPEED);

    OUTREG(SDVOB, sdvob);
    OUTREG(SDVOC, sdvoc);
}

static void
i830_sdvo_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (mode != DPMSModeOn) {
	i830_sdvo_set_active_outputs(output, FALSE, FALSE);
	OUTREG(SDVOB, INREG(SDVOB) & ~SDVO_ENABLE);
    } else {
	i830_sdvo_set_active_outputs(output, TRUE, FALSE);
	OUTREG(SDVOB, INREG(SDVOB) | SDVO_ENABLE);
    }
}

static void
i830_sdvo_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    dev_priv->save_sdvo_mult = i830_sdvo_get_clock_rate_mult(output);
    i830_sdvo_get_active_outputs(output, &dev_priv->save_sdvo_active_1,
				 &dev_priv->save_sdvo_active_2);

    if (dev_priv->caps.caps & 0x1) {
       i830_sdvo_set_target_input(output, FALSE, FALSE);
       i830_sdvo_get_timings(output, &dev_priv->save_input_dtd_1,
			     SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.caps & 0x2) {
       i830_sdvo_set_target_input(output, FALSE, TRUE);
       i830_sdvo_get_timings(output, &dev_priv->save_input_dtd_2,
			     SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.output_0_supported) {
       i830_sdvo_set_target_output(output, TRUE, FALSE);
       i830_sdvo_get_timings(output, &dev_priv->save_output_dtd_1,
			     SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.output_1_supported) {
       i830_sdvo_set_target_output(output, FALSE, TRUE);
       i830_sdvo_get_timings(output, &dev_priv->save_output_dtd_2,
			     SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    }

    dev_priv->save_SDVOX = INREG(dev_priv->output_device);
}

static void
i830_sdvo_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    if (dev_priv->caps.caps & 0x1) {
       i830_sdvo_set_target_input(output, FALSE, FALSE);
       i830_sdvo_set_timings(output, &dev_priv->save_input_dtd_1,
			     SDVO_CMD_SET_INPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.caps & 0x2) {
       i830_sdvo_set_target_input(output, FALSE, TRUE);
       i830_sdvo_set_timings(output, &dev_priv->save_input_dtd_2,
			     SDVO_CMD_SET_INPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.output_0_supported) {
       i830_sdvo_set_target_output(output, TRUE, FALSE);
       i830_sdvo_set_timings(output, &dev_priv->save_output_dtd_1,
			     SDVO_CMD_SET_OUTPUT_TIMINGS_PART1);
    }

    if (dev_priv->caps.output_1_supported) {
       i830_sdvo_set_target_output(output, FALSE, TRUE);
       i830_sdvo_set_timings(output, &dev_priv->save_output_dtd_2,
			     SDVO_CMD_SET_OUTPUT_TIMINGS_PART1);
    }

    i830_sdvo_set_clock_rate_mult(output, dev_priv->save_sdvo_mult);

    OUTREG(dev_priv->output_device, dev_priv->save_SDVOX);

    i830_sdvo_set_active_outputs(output, dev_priv->save_sdvo_active_1,
				 dev_priv->save_sdvo_active_2);
}

static int
i830_sdvo_mode_valid(ScrnInfoPtr pScrn, I830OutputPtr output,
		     DisplayModePtr pMode)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    if (dev_priv->pixel_clock_min > pMode->Clock)
	return MODE_CLOCK_HIGH;

    if (dev_priv->pixel_clock_max < pMode->Clock)
	return MODE_CLOCK_LOW;

    return MODE_OK;
}

static void
i830_sdvo_get_capabilities(I830OutputPtr output, i830_sdvo_caps *caps)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, 9);
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_DEVICE_CAPS;
    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    caps->vendor_id = dev_priv->sdvo_regs[SDVO_I2C_RETURN_0];
    caps->device_id = dev_priv->sdvo_regs[SDVO_I2C_RETURN_1];
    caps->device_rev_id = dev_priv->sdvo_regs[SDVO_I2C_RETURN_2];
    caps->sdvo_version_major = dev_priv->sdvo_regs[SDVO_I2C_RETURN_3];
    caps->sdvo_version_minor = dev_priv->sdvo_regs[SDVO_I2C_RETURN_4];
    caps->caps = dev_priv->sdvo_regs[SDVO_I2C_RETURN_5];
    caps->output_0_supported = dev_priv->sdvo_regs[SDVO_I2C_RETURN_6];
    caps->output_1_supported = dev_priv->sdvo_regs[SDVO_I2C_RETURN_7];
}

/** Forces the device over to the real I2C bus and uses its GetByte */
static Bool
i830_sdvo_ddc_i2c_get_byte(I2CDevPtr d, I2CByte *data, Bool last)
{
    I830OutputPtr output = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = output->pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CGetByte(d, data, last);
    d->pI2CBus = savebus;

    return ret;
}

/** Forces the device over to the real I2C bus and uses its PutByte */
static Bool
i830_sdvo_ddc_i2c_put_byte(I2CDevPtr d, I2CByte c)
{
    I830OutputPtr output = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = output->pI2CBus, savebus;
    Bool ret;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    ret = i2cbus->I2CPutByte(d, c);
    d->pI2CBus = savebus;

    return ret;
}

/**
 * Sets the control bus over to DDC before sending the start on the real I2C
 * bus.
 *
 * The control bus will flip back at the stop following the start executed
 * here.
 */
static Bool
i830_sdvo_ddc_i2c_start(I2CBusPtr b, int timeout)
{
    I830OutputPtr output = b->DriverPrivate.ptr;
    I2CBusPtr i2cbus = output->pI2CBus;

    i830_sdvo_set_control_bus_switch(output, SDVO_CONTROL_BUS_DDC2);
    return i2cbus->I2CStart(i2cbus, timeout);
}

/** Forces the device over to the real SDVO bus and sends a stop to it. */
static void
i830_sdvo_ddc_i2c_stop(I2CDevPtr d)
{
    I830OutputPtr output = d->pI2CBus->DriverPrivate.ptr;
    I2CBusPtr i2cbus = output->pI2CBus, savebus;

    savebus = d->pI2CBus;
    d->pI2CBus = i2cbus;
    i2cbus->I2CStop(d);
    d->pI2CBus = savebus;
}

/**
 * Mirrors xf86i2c I2CAddress, using the bus's (wrapped) methods rather than
 * the default methods.
 *
 * This ensures that our start commands always get wrapped with control bus
 * switches.  xf86i2c should probably be fixed to do this.
 */
static Bool
i830_sdvo_ddc_i2c_address(I2CDevPtr d, I2CSlaveAddr addr)
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

static void
i830_sdvo_dump_cmd(I830OutputPtr output, int opcode)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    memset(dev_priv->sdvo_regs, 0, sizeof(dev_priv->sdvo_regs));
    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = opcode;
    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);
}

static void
i830_sdvo_dump_device(I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    ErrorF("Dump %s\n", dev_priv->d.DevName);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_DEVICE_CAPS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_FIRMWARE_REV);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_TRAINED_INPUTS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_ACTIVE_OUTPUTS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_IN_OUT_MAP);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_ATTACHED_DISPLAYS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_HOT_PLUG_SUPPORT);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_ACTIVE_HOT_PLUG);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_INTR_EVENT_SOURCE);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_INPUT_TIMINGS_PART1);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_INPUT_TIMINGS_PART2);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_OUTPUT_TIMINGS_PART1);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_OUTPUT_TIMINGS_PART2);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_OUTPUT_PIXEL_CLOCK_RANGE);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_SUPPORTED_CLOCK_RATE_MULTS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_CLOCK_RATE_MULT);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_SUPPORTED_TV_FORMATS);
    i830_sdvo_dump_cmd(output, SDVO_CMD_GET_TV_FORMAT);
}

void
i830_sdvo_dump(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int	i;

    for (i = 0; i < pI830->num_outputs; i++) {
	if (pI830->output[i].type == I830_OUTPUT_SDVO)
	    i830_sdvo_dump_device(&pI830->output[i]);
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
i830_sdvo_detect_displays(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    struct i830_sdvo_priv *dev_priv = output->dev_priv;

    dev_priv->sdvo_regs[SDVO_I2C_OPCODE] = SDVO_CMD_GET_ATTACHED_DISPLAYS;
    i830_sdvo_write_outputs(output, 0);
    i830_sdvo_read_input_regs(output);

    if (dev_priv->sdvo_regs[SDVO_I2C_CMD_STATUS] != SDVO_CMD_STATUS_SUCCESS)
	return FALSE;

    return (dev_priv->sdvo_regs[SDVO_I2C_RETURN_0] != 0 ||
	    dev_priv->sdvo_regs[SDVO_I2C_RETURN_1] != 0);
}

void
i830_sdvo_init(ScrnInfoPtr pScrn, int output_device)
{
    I830Ptr pI830 = I830PTR(pScrn);
    I830OutputPtr output = &pI830->output[pI830->num_outputs];
    struct i830_sdvo_priv *dev_priv;
    int i;
    unsigned char ch[0x40];
    I2CBusPtr i2cbus = NULL, ddcbus;

    output->type = I830_OUTPUT_SDVO;
    output->dpms = i830_sdvo_dpms;
    output->save = i830_sdvo_save;
    output->restore = i830_sdvo_restore;
    output->mode_valid = i830_sdvo_mode_valid;
    output->pre_set_mode = i830_sdvo_pre_set_mode;
    output->post_set_mode = i830_sdvo_post_set_mode;

    /* While it's the same bus, we just initialize a new copy to avoid trouble
     * with tracking refcounting ourselves, since the XFree86 DDX bits don't.
     */
    if (output_device == SDVOB)
	I830I2CInit(pScrn, &i2cbus, GPIOE, "SDVOCTRL_E for SDVOB");
    else
	I830I2CInit(pScrn, &i2cbus, GPIOE, "SDVOCTRL_E for SDVOC");

    if (i2cbus == NULL)
	return;

    /* Allocate the SDVO output private data */
    dev_priv = xcalloc(1, sizeof(struct i830_sdvo_priv));
    if (dev_priv == NULL) {
	xf86DestroyI2CBusRec(i2cbus, TRUE, TRUE);
	return;
    }

    if (output_device == SDVOB) {
	dev_priv->d.DevName = "SDVO Controller B";
	dev_priv->d.SlaveAddr = 0x70;
    } else {
	dev_priv->d.DevName = "SDVO Controller C";
	dev_priv->d.SlaveAddr = 0x72;
    }
    dev_priv->d.pI2CBus = i2cbus;
    dev_priv->d.DriverPrivate.ptr = output;
    dev_priv->output_device = output_device;

    if (!xf86I2CDevInit(&dev_priv->d)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Failed to initialize SDVO I2C device %s\n",
		   output_device == SDVOB ? "SDVOB" : "SDVOC");
	xf86DestroyI2CBusRec(i2cbus, TRUE, TRUE);
	xfree(dev_priv);
	return;
    }

    /* Set up our wrapper I2C bus for DDC.  It acts just like the regular I2C
     * bus, except that it does the control bus switch to DDC mode before every
     * Start.  While we only need to do it at Start after every Stop after a
     * Start, extra attempts should be harmless.
     */
    ddcbus = xf86CreateI2CBusRec();
    if (ddcbus == NULL) {
	xf86DestroyI2CDevRec(&dev_priv->d, FALSE);
	xf86DestroyI2CBusRec(i2cbus, TRUE, TRUE);
	xfree(dev_priv);
	return;
    }
    if (output_device == SDVOB)
        ddcbus->BusName = "SDVOB DDC Bus";
    else
        ddcbus->BusName = "SDVOC DDC Bus";
    ddcbus->scrnIndex = i2cbus->scrnIndex;
    ddcbus->I2CGetByte = i830_sdvo_ddc_i2c_get_byte;
    ddcbus->I2CPutByte = i830_sdvo_ddc_i2c_put_byte;
    ddcbus->I2CStart = i830_sdvo_ddc_i2c_start;
    ddcbus->I2CStop = i830_sdvo_ddc_i2c_stop;
    ddcbus->I2CAddress = i830_sdvo_ddc_i2c_address;
    ddcbus->DriverPrivate.ptr = &pI830->output[pI830->num_outputs];
    if (!xf86I2CBusInit(ddcbus)) {
	xf86DestroyI2CDevRec(&dev_priv->d, FALSE);
	xf86DestroyI2CBusRec(i2cbus, TRUE, TRUE);
	xfree(dev_priv);
	return;
    }

    output->pI2CBus = i2cbus;
    output->pDDCBus = ddcbus;
    output->dev_priv = dev_priv;

    /* Read the regs to test if we can talk to the device */
    for (i = 0; i < 0x40; i++) {
	if (!i830_sdvo_read_byte(output, i, &ch[i])) {
	    xf86DestroyI2CBusRec(output->pDDCBus, FALSE, FALSE);
	    xf86DestroyI2CDevRec(&dev_priv->d, FALSE);
	    xf86DestroyI2CBusRec(i2cbus, TRUE, TRUE);
	    xfree(dev_priv);
	    return;
	}
    }

    i830_sdvo_get_capabilities(output, &dev_priv->caps);

    i830_sdvo_get_input_pixel_clock_range(output, &dev_priv->pixel_clock_min,
					  &dev_priv->pixel_clock_max);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "SDVO device VID/DID: %02X:%02X.%02X, %02X,"
	       "output 1: %c, output 2: %c\n",
	       dev_priv->caps.vendor_id, dev_priv->caps.device_id,
	       dev_priv->caps.device_rev_id, dev_priv->caps.caps,
	       dev_priv->caps.output_0_supported ? 'Y' : 'N',
	       dev_priv->caps.output_1_supported ? 'Y' : 'N');

    pI830->num_outputs++;
}
