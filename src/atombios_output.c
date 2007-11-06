/*
 * Copyright © 2007 Dave Airlie
 */
/*
 * avivo output handling functions. 
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DPMS */
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#include <unistd.h>

#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_atombios.h"

static
Bool AVIVOI2CDoLock(ScrnInfoPtr pScrn, int lock_state);

static AtomBiosResult atom_bios_display_device_control(atomBIOSHandlePtr atomBIOS, int device, Bool state)
{
    DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION disp_data;
    AtomBIOSArg data;
    unsigned char *space;
    AtomBiosResult ret;

    disp_data.ucAction = state;
    data.exec.index = device;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output %d enable success\n", device);
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output %d enable failed\n", device);
    return ATOM_NOT_IMPLEMENTED;
}

static void
atom_bios_enable_crt(atomBIOSHandlePtr atomBIOS, int dac, Bool state)
{
    int output;
    if (dac == DAC_PRIMARY)
	output = GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
    else
	output = GetIndexIntoMasterTable(COMMAND, DAC2OutputControl);

    atom_bios_display_device_control(atomBIOS, output, state);
}

static int
atombios_output_dac_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBIOSArg data;
    unsigned char *space;
    AtomBiosResult ret;

    disp_data.ucAction = 1;
    disp_data.ucDacStandard = 1;
    disp_data.usPixelClock = mode->Clock / 10;
    if (radeon_output->DACType == DAC_PRIMARY)
	data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
    else
	data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBIOSFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC %d enable success\n", radeon_output->DACType);
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output DAC %d enable failed\n", radeon_output->DACType);
    return ATOM_NOT_IMPLEMENTED;

    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, CRT1OutputControl), ATOM_TRANSMITTER_ACTION_INIT);
    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, CRT1OutputControl), ATOM_TRANSMITTER_ACTION_SETUP);
    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, CRT1OutputControl), ATOM_TRANSMITTER_ACTION_ENABLE_OUTPUT);
    return ATOM_SUCCESS;

}


static int
atombios_output_tmds1_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int tmp;
    TMDS1_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBIOSArg data;
    unsigned char *space;
    AtomBiosResult ret;

    disp_data.ucAction = 1;
    if (mode->Clock > 165000)
	disp_data.ucMisc = 1;
    else
	disp_data.ucMisc = 0;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, TMDS1EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBIOSFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output TMDS1 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS1 enable failed\n");
    return ATOM_NOT_IMPLEMENTED;

    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, DFP1OutputControl), ATOM_TRANSMITTER_ACTION_INIT);
    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, DFP1OutputControl), ATOM_TRANSMITTER_ACTION_SETUP);
    atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, DFP1OutputControl), ATOM_TRANSMITTER_ACTION_ENABLE_OUTPUT);
    return ATOM_SUCCESS;
}

static void
atombios_output_tmds2_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned int tmp;
    TMDS2_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBIOSArg data;
    unsigned char *space;
    AtomBiosResult ret;

    disp_data.ucAction = 1;
    if (mode->Clock > 165000)
	disp_data.ucMisc = 1;
    else
	disp_data.ucMisc = 0;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, TMDS2EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBIOSFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output TMDS2 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS2 enable failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static void
atombios_output_dac_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);

    switch(mode) {
    case DPMSModeOn:
	atom_bios_enable_crt(info->atomBIOS, radeon_output->DACType, ATOM_ENABLE);
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	atom_bios_enable_crt(info->atomBIOS, radeon_output->DACType, ATOM_DISABLE);
	break;
    }
}

static void
atombios_output_tmds1_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr avivo_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);

    switch(mode) {
    case DPMSModeOn:
	/* TODO */
	atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl), ATOM_ENABLE);
    
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	/* TODO */
	atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl), ATOM_DISABLE);
        break;
    }
}

static void
atombios_output_tmds2_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr avivo_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;

    switch(mode) {
    case DPMSModeOn:
	atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl), ATOM_ENABLE);
	/* TODO */
        break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	atom_bios_display_device_control(info->atomBIOS, GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl), ATOM_DISABLE);
	/* TODO */
        break;
    }
}

static void
atombios_output_lvds_dpms(xf86OutputPtr output, int mode)
{
    atombios_output_tmds2_dpms(output, mode);
}

void
atombios_output_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr avivo_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int tmp, count;

#if 1
    /* try to grab card lock or at least somethings that looks like a lock
     * if it fails more than 5 times with 1000ms wait btw each try than we
     * assume we can process.
     */
    count = 0;
    tmp = INREG(0x0028);
    while((tmp & 0x100) && (count < 5)) {
        tmp = INREG(0x0028);
        count++;
        usleep(1000);
    }
    if (count >= 5) {
        xf86DrvMsg(output->scrn->scrnIndex, X_INFO,
                   "%s (WARNING) failed to grab card lock process anyway.\n",
                   __func__);
    }
    OUTREG(0x0028, tmp | 0x100);
#endif

    ErrorF("AGD: output dpms\n");

   if (avivo_output->MonType == MT_LCD) {
       atombios_output_tmds2_dpms(output, mode);
   } else if (avivo_output->MonType == MT_DFP) {
       ErrorF("AGD: tmds dpms\n");
       atombios_output_tmds1_dpms(output, mode);
   } else if (avivo_output->MonType == MT_CRT) {
       ErrorF("AGD: dac dpms\n");
       atombios_output_dac_dpms(output, mode);
   }

#if 1
    /* release card lock */
    tmp = INREG(0x0028);
    OUTREG(0x0028, tmp & (~0x100));
#endif
}

static int
atombios_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    if (pMode->Flags & V_DBLSCAN)
        return MODE_NO_DBLESCAN;

    if (pMode->Clock > 400000 || pMode->Clock < 25000)
        return MODE_CLOCK_RANGE;

    return MODE_OK;
}

static Bool
atombios_output_mode_fixup(xf86OutputPtr output,
                        DisplayModePtr mode,
                        DisplayModePtr adjusted_mode)
{
    return TRUE;
}

static void
atombios_output_prepare(xf86OutputPtr output)
{
    output->funcs->dpms(output, DPMSModeOff);
}



void
atombios_output_mode_set(xf86OutputPtr output,
                      DisplayModePtr mode,
                      DisplayModePtr adjusted_mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if (radeon_output->MonType == MT_CRT) {
	if (radeon_output->DACType == DAC_PRIMARY) {
	    ErrorF("AGD: atom dac setup\n");
	    atombios_output_dac_setup(output, adjusted_mode);
	}
    } else if (radeon_output->MonType == MT_DFP) {
	ErrorF("AGD: atom tmds setup\n");
	if (radeon_output->TMDSType == TMDS_INT) 
	    atombios_output_tmds1_setup(output, adjusted_mode);
	else
	    atombios_output_tmds2_setup(output, adjusted_mode);
    } else if (radeon_output->MonType == MT_LCD) {
	atombios_output_tmds2_setup(output, adjusted_mode);
    }
}

static void
atombios_output_commit(xf86OutputPtr output)
{
    output->funcs->dpms(output, DPMSModeOn);
}

DisplayModePtr
atombios_output_get_modes(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr atombios_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86MonPtr edid_mon;
    DisplayModePtr modes;

    modes = RADEONProbeOutputModes(output);
    return modes;
}

static void
atombios_output_destroy(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr avivo_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (avivo_output == NULL)
        return;
    xf86DestroyI2CBusRec(avivo_output->pI2CBus, TRUE, TRUE);
    xfree(avivo_output->name);
    xfree(avivo_output);
}


Bool
atombios_output_lfp_mode_fixup(xf86OutputPtr output,
                            DisplayModePtr mode,
                            DisplayModePtr adjusted_mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);

#if 0
    if (avivo->lfp_fixed_mode) {
        adjusted_mode->HDisplay = info->lfp_fixed_mode->HDisplay;
        adjusted_mode->HSyncStart = info->lfp_fixed_mode->HSyncStart;
        adjusted_mode->HSyncEnd = info->lfp_fixed_mode->HSyncEnd;
        adjusted_mode->HTotal = info->lfp_fixed_mode->HTotal;
        adjusted_mode->VDisplay = info->lfp_fixed_mode->VDisplay;
        adjusted_mode->VSyncStart = info->lfp_fixed_mode->VSyncStart;
        adjusted_mode->VSyncEnd = info->lfp_fixed_mode->VSyncEnd;
        adjusted_mode->VTotal = info->lfp_fixed_mode->VTotal;
        adjusted_mode->Clock = info->lfp_fixed_mode->Clock;
        xf86SetModeCrtc(adjusted_mode, 0);
    }
#endif
    return TRUE;
}

DisplayModePtr
atombios_output_lfp_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr screen_info = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DisplayModePtr modes = NULL;
   
    modes = atombios_output_get_modes(output);
    if (modes == NULL) {
        /* DDC EDID failed try to get timing from BIOS */
        xf86DrvMsg(screen_info->scrnIndex, X_WARNING,
                   "Failed to get EDID over i2c for LFP try BIOS timings.\n");
        modes = atombios_bios_get_lfp_timing(screen_info);
    }
#if 0
    if (modes) {
        xf86DeleteMode(&info->lfp_fixed_mode, info->lfp_fixed_mode);
        info->lfp_fixed_mode = xf86DuplicateMode(modes);
    }
#endif
    return modes;
}



void
atombios_i2c_gpio0_get_bits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr screen_info = xf86Screens[b->scrnIndex]; 
    RADEONInfoPtr info       = RADEONPTR(screen_info);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  val;

    ErrorF("INREG %08x\n", INREG(b->DriverPrivate.uval));
    /* Get the result */
    val = INREG(b->DriverPrivate.uval + 0xC);
    *Clock = (val & (1<<19)) != 0;
    *data  = (val & (1<<18)) != 0;
}

void 
atombios_i2c_gpio0_put_bits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr screen_info = xf86Screens[b->scrnIndex]; 
    RADEONInfoPtr info       = RADEONPTR(screen_info);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  val;

    val = 0;
    val |= (Clock ? 0:(1<<19));
    val |= (data ? 0:(1<<18));
    OUTREG(b->DriverPrivate.uval + 0x8, val);
    /* read back to improve reliability on some cards. */
    val = INREG(b->DriverPrivate.uval + 0x8);
}

void
atombios_i2c_gpio123_get_bits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr screen_info = xf86Screens[b->scrnIndex]; 
    RADEONInfoPtr info       = RADEONPTR(screen_info);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  val;

    if (INREG(b->DriverPrivate.uval) == 0)
	OUTREG(b->DriverPrivate.uval, (1<<0) | (1<<8));

    /* Get the result */
    val = INREG(b->DriverPrivate.uval + 0xC);
    *Clock = (val & (1<<0)) != 0;
    *data  = (val & (1<<8)) != 0;
}

void 
atombios_i2c_gpio123_put_bits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr screen_info = xf86Screens[b->scrnIndex]; 
    RADEONInfoPtr info       = RADEONPTR(screen_info);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long  val;

    val = 0;
    val |= (Clock ? 0:(1<<0));
    val |= (data ? 0:(1<<8));
    OUTREG(b->DriverPrivate.uval + 0x8, val);
    /* read back to improve reliability on some cards. */
    val = INREG(b->DriverPrivate.uval + 0x8);
}

static xf86OutputStatus
atombios_output_detect(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr avivo_output = output->driver_private;
    ScrnInfoPtr screen_info = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    AtomBiosResult ret;
    uint32_t bios_0_scratch;

    return radeon_detect(output);
}

static const xf86OutputFuncsRec atombios_output_dac_funcs = {
    .dpms = atombios_output_dpms,
    .save = NULL,
    .restore = NULL,
    .mode_valid = atombios_output_mode_valid,
    .mode_fixup = atombios_output_mode_fixup,
    .prepare = atombios_output_prepare,
    .mode_set = atombios_output_mode_set,
    .commit = atombios_output_commit,
    .detect = atombios_output_detect,
    .get_modes = atombios_output_get_modes,
    .destroy = atombios_output_destroy
};

static const xf86OutputFuncsRec atombios_output_tmds_funcs = {
    .dpms = atombios_output_dpms,
    .save = NULL,
    .restore = NULL,
    .mode_valid = atombios_output_mode_valid,
    .mode_fixup = atombios_output_mode_fixup,
    .prepare = atombios_output_prepare,
    .mode_set = atombios_output_mode_set,
    .commit = atombios_output_commit,
    .detect = atombios_output_detect,
    .get_modes = atombios_output_get_modes,
    .destroy = atombios_output_destroy
};

static const xf86OutputFuncsRec atombios_output_lfp_funcs = {
    .dpms = atombios_output_dpms,
    .save = NULL,
    .restore = NULL,
    .mode_valid = atombios_output_mode_valid,
    .mode_fixup = atombios_output_lfp_mode_fixup,
    .prepare = atombios_output_prepare,
    .mode_set = atombios_output_mode_set,
    .commit = atombios_output_commit,
    .detect = atombios_output_detect,
    .get_modes = atombios_output_get_modes,
    .destroy = atombios_output_destroy
};

Bool
atombios_output_exist(ScrnInfoPtr screen_info, xf86ConnectorType type,
                  int number, unsigned long ddc_reg)
{
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(screen_info);
    int i;

    for (i = 0; i < config->num_output; i++) {
        xf86OutputPtr output = config->output[i];
        RADEONOutputPrivatePtr avivo_output = output->driver_private;
        if (avivo_output->num == number && avivo_output->type == type)
            return TRUE;
        /* LVTMA is shared by LFP & DVI-I */
        if (avivo_output->type == XF86ConnectorLFP && number >= 1)
            return TRUE;
        if (type == XF86ConnectorLFP && avivo_output->num >= 1) {
            avivo_output->type = type;
            avivo_output->pI2CBus->DriverPrivate.uval = ddc_reg;
            return TRUE;
        }
    }
    return FALSE;
}

#if 0
Bool
avivo_output_init(ScrnInfoPtr screen_info, xf86ConnectorType type,
                  int number, unsigned long ddc_reg)
{
    xf86OutputPtr output = {0,};
    RADEONOutputPrivateRec *avivo_output;
    int name_size;

    /* allocate & initialize private output structure */
    avivo_output = xcalloc(sizeof(RADEONOutputPrivateRec), 1);
    if (avivo_output == NULL)
        return FALSE;
    name_size = snprintf(NULL, 0, "%s connector %d",
                         xf86ConnectorGetName(type), number);
    avivo_output->name = xcalloc(name_size + 1, 1);
    if (avivo_output->name == NULL) {
        xfree(avivo_output);
        xf86DrvMsg(screen_info->scrnIndex, X_ERROR,
                   "Failed to allocate memory for I2C bus name\n");
        return FALSE;
    }
    snprintf(avivo_output->name, name_size + 1, "%s connector %d",
             xf86ConnectorGetName(type), number);
    avivo_output->pI2CBus = xf86CreateI2CBusRec();
    if (!avivo_output->pI2CBus) {
        xfree(avivo_output);
        xf86DrvMsg(screen_info->scrnIndex, X_ERROR,
                   "Couldn't create I2C bus for %s connector %d\n",
                   xf86ConnectorGetName(type), number);
        return FALSE;
    }
    avivo_output->pI2CBus->BusName = avivo_output->name;
    avivo_output->pI2CBus->scrnIndex = screen_info->scrnIndex;
    if (ddc_reg == AVIVO_GPIO_0) {
        avivo_output->pI2CBus->I2CPutBits = atombios_i2c_gpio0_put_bits;
        avivo_output->pI2CBus->I2CGetBits = atombios_i2c_gpio0_get_bits;
    } else {
        avivo_output->pI2CBus->I2CPutBits = avivo_i2c_gpio123_put_bits;
        avivo_output->pI2CBus->I2CGetBits = avivo_i2c_gpio123_get_bits;
    }
    avivo_output->pI2CBus->AcknTimeout = 5;
    avivo_output->pI2CBus->DriverPrivate.uval = ddc_reg;
    if (!xf86I2CBusInit(avivo_output->pI2CBus)) {
        xf86DrvMsg(screen_info->scrnIndex, X_ERROR,
                   "Couldn't initialise I2C bus for %s connector %d\n",
                   xf86ConnectorGetName(type), number);
        return FALSE;
    }
    avivo_output->gpio = ddc_reg;
    avivo_output->type = type;
    avivo_output->num = number;
    switch (avivo_output->type) {
    case XF86ConnectorVGA:
	avivo_output->setup = avivo_output_dac_setup;
	avivo_output->dpms = avivo_output_dac_dpms;
        output = xf86OutputCreate (screen_info,
                                   &avivo_output_dac_funcs,
                                   xf86ConnectorGetName(type));
        break;
    case XF86ConnectorLFP:
        avivo_output->setup = avivo_output_tmds2_setup;
        avivo_output->dpms = avivo_output_lvds_dpms;
        output = xf86OutputCreate (screen_info,
                                   &avivo_output_lfp_funcs,
                                   xf86ConnectorGetName(type));
        break;
    case XF86ConnectorDVI_I:
    case XF86ConnectorDVI_D:
    case XF86ConnectorDVI_A:
        if (!number) {
            avivo_output->setup = avivo_output_tmds1_setup;
            avivo_output->dpms = avivo_output_tmds1_dpms;
        } else {
            avivo_output->setup = avivo_output_tmds2_setup;
            avivo_output->dpms = avivo_output_tmds2_dpms;
        }
        output = xf86OutputCreate (screen_info,
                                   &avivo_output_tmds_funcs,
                                   xf86ConnectorGetName(type));
        break;
    default:
        avivo_output->setup = NULL;
        break;
    }

    if (output == NULL) {
        xf86DestroyI2CBusRec(avivo_output->pI2CBus, TRUE, TRUE);
        xfree(avivo_output);
        return FALSE;
    }
    output->driver_private = avivo_output;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;
    xf86DrvMsg(screen_info->scrnIndex, X_INFO,
               "added %s connector %d (0x%04lX)\n",
               xf86ConnectorGetName(type), number, ddc_reg);

	return TRUE;
}
#endif
extern void RADEONSetOutputType(ScrnInfoPtr pScrn, RADEONOutputPrivatePtr radeon_output);
extern void RADEONInitConnector(xf86OutputPtr output);
extern const char *OutputType[], *DDCTypeName[];


static
Bool AVIVOI2CReset(ScrnInfoPtr pScrn)
{
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;

  OUTREG(AVIVO_I2C_STOP, 1);
  INREG(AVIVO_I2C_STOP);
  OUTREG(AVIVO_I2C_STOP, 0x0);
  return TRUE;
}

static
Bool AVIVOI2CDoLock(ScrnInfoPtr pScrn, int lock_state)
{
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  CARD32 temp;
  int count=0;

  switch(lock_state) {
  case 0:
    temp = INREG(AVIVO_I2C_CNTL);
    OUTREG(AVIVO_I2C_CNTL, temp | 0x100);
    /* enable hdcp block */
    OUTREG(R520_PCLK_HDCP_CNTL, 0x0);
    break;
  case 1:
    /* disable hdcp block */
    OUTREG(R520_PCLK_HDCP_CNTL, 0x1);
    usleep(1);
    OUTREG(AVIVO_I2C_CNTL, 0x1);
    usleep(1);
    temp = INREG(AVIVO_I2C_CNTL);
    if (!(temp & 0x2)) {
      ErrorF("Lock failed %08X\n", temp);
      return FALSE;
    }
    break;
  }
  return TRUE;
}

#if 0
static Bool
AVIVOI2CSendData(I2CDevPtr d, int address, int nWrite, I2CByte *data)
{
  I2CBusPtr b = d->pI2CBus;
  ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  CARD32 temp;
  int count, i;

  OUTREG(AVIVO_I2C_STATUS, (AVIVO_I2C_STATUS_DONE | AVIVO_I2C_STATUS_NACK | AVIVO_I2C_STATUS_HALT));
  temp = INREG(AVIVO_I2C_START_CNTL);
  temp |= R520_I2C_START | R520_I2C_STOP | R520_I2C_RX | R520_I2C_EN;

  temp &= ~R520_I2C_DDC_MASK;
  
  switch(b->DriverPrivate.uval)
    {
    case 0x7e40:
      temp |= R520_I2C_DDC1;
    break;
    case 0x7e50:
    default:
      temp |= R520_I2C_DDC2;
    break;
  }

  OUTREG(AVIVO_I2C_START_CNTL, temp);
  
  temp = INREG(AVIVO_I2C_CONTROL2);
  temp &= ~R520_I2C_DATA_COUNT_MASK;
  temp |= 1 << R520_I2C_DATA_COUNT_SHIFT;
  temp &= ~R520_I2C_ADDR_COUNT_MASK;
  temp |= 1;
  OUTREG(AVIVO_I2C_CONTROL2, temp);
  
  temp = INREG(AVIVO_I2C_CONTROL3);
  OUTREG(AVIVO_I2C_CONTROL3, temp);
  
  OUTREG(AVIVO_I2C_DATA, address);
  for (i=0; i<nWrite; i++)
    OUTREG(AVIVO_I2C_DATA, data[i]);
  
  /* set to i2c tx mode */
  temp = INREG(AVIVO_I2C_START_CNTL);
  temp &= ~R520_I2C_RX;
  OUTREG(AVIVO_I2C_START_CNTL, temp);
  
  /* set go flag */
  OUTREG(AVIVO_I2C_STATUS, AVIVO_I2C_STATUS_GO);
  
  count = 0;
  do {
    temp = INREG(AVIVO_I2C_STATUS);
    if (temp & AVIVO_I2C_STATUS_DONE)
      break;
    usleep(1);
    count++;
  } while(count<10);

  if (count == 10)
    return FALSE;
  OUTREG(AVIVO_I2C_STATUS, temp);
  
  return TRUE;
}
static Bool
AVIVOI2CWriteRead(I2CDevPtr d, I2CByte *WriteBuffer, int nWrite,
		      I2CByte *ReadBuffer, int nRead)
{
  I2CBusPtr b = d->pI2CBus;
  ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  CARD32 temp;
  int i, count;
  int sofar, thisread;
  I2CByte offbuf[1];
  Bool ret;

  AVIVOI2CReset(pScrn);

  /* set the control1 flags */
  if (nWrite > 1)
  {
    ret = AVIVOI2CSendData(d, d->SlaveAddr, nWrite, WriteBuffer);
    if (ret==FALSE)
      return FALSE;
  }

  if (nRead > 0 && nWrite == 1)
  {
    /* okay this is a standard read - the i2c hw can only do 15 bytes */
    sofar = 0;
    do {
      thisread = nRead - sofar;
      if (thisread > 15)
	thisread = 15;

      offbuf[0] = sofar;
      ret = AVIVOI2CSendData(d, d->SlaveAddr, 1, offbuf);
      if (ret==FALSE)
	return FALSE;

      OUTREG(AVIVO_I2C_DATA, d->SlaveAddr | 0x1);
      
      temp = INREG(AVIVO_I2C_START_CNTL);
      temp |= R520_I2C_RX;
      OUTREG(AVIVO_I2C_START_CNTL, temp);
 
      temp = INREG(AVIVO_I2C_CONTROL2);
      temp &= ~R520_I2C_DATA_COUNT_MASK;
      temp |= thisread << R520_I2C_DATA_COUNT_SHIFT;
      temp &= ~R520_I2C_ADDR_COUNT_MASK;
      temp |= 1;
      OUTREG(AVIVO_I2C_CONTROL2, temp);

      OUTREG(AVIVO_I2C_STATUS, AVIVO_I2C_STATUS_GO);       
      count = 0;
      do {
	temp = INREG(AVIVO_I2C_STATUS);
	if (temp & AVIVO_I2C_STATUS_DONE)
	  break;
	usleep(1);
	count++;
      } while(count<100);
      if (count == 100)
	return FALSE;

      OUTREG(AVIVO_I2C_STATUS, temp);
      
      for (i=0; i<thisread; i++)
      {
	temp = INREG(AVIVO_I2C_DATA);
	ReadBuffer[sofar+i] = (I2CByte)(temp & 0xff);
      }
      sofar += thisread;
    } while(sofar < nRead);
  }
  return TRUE;
}


Bool atombios_i2c_init(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
  RADEONInfoPtr info = RADEONPTR(pScrn);
  I2CBusPtr pI2CBus;

  pI2CBus = xf86CreateI2CBusRec();
  if (!pI2CBus)
    return FALSE;

  pI2CBus->BusName = name;
  pI2CBus->scrnIndex = pScrn->scrnIndex;
  pI2CBus->I2CWriteRead = AVIVOI2CWriteRead;
  pI2CBus->DriverPrivate.uval = i2c_reg;
  
  ErrorF("uval is %04X\n", i2c_reg);
  if (!xf86I2CBusInit(pI2CBus))
    return FALSE;

  *bus_ptr = pI2CBus;
  return TRUE;
}

#else
Bool
atom_bios_i2c_init(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
    I2CBusPtr pI2CBus;

    pI2CBus = xf86CreateI2CBusRec();
    if (!pI2CBus) return FALSE;

    pI2CBus->BusName    = name;
    pI2CBus->scrnIndex  = pScrn->scrnIndex;
    if (i2c_reg == AVIVO_GPIO_0) {
      pI2CBus->I2CPutBits = atombios_i2c_gpio0_put_bits;
      pI2CBus->I2CGetBits = atombios_i2c_gpio0_get_bits;
    } else {
      pI2CBus->I2CPutBits = atombios_i2c_gpio123_put_bits;
      pI2CBus->I2CGetBits = atombios_i2c_gpio123_get_bits;
    }
    pI2CBus->AcknTimeout = 5;
    pI2CBus->DriverPrivate.uval = i2c_reg;

    if (!xf86I2CBusInit(pI2CBus)) return FALSE;

    *bus_ptr = pI2CBus;
    return TRUE;
}
#endif

void atombios_init_connector(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int DDCReg = 0;
    char* name = (char*) DDCTypeName[radeon_output->DDCType];

    if (radeon_output->gpio) {
        radeon_output->DDCReg = radeon_output->gpio;
	if (IS_AVIVO_VARIANT)
	    atom_bios_i2c_init(pScrn, &radeon_output->pI2CBus, radeon_output->gpio, output->name);
	else
	    RADEONI2CInit(pScrn, &radeon_output->pI2CBus, radeon_output->DDCReg, name);
    }

    if (radeon_output->type == OUTPUT_LVDS) {
	RADEONGetLVDSInfo(output);
    }

    if (radeon_output->type == OUTPUT_DVI) {
      //	RADEONGetTMDSInfo(output);
    }

    if (radeon_output->type == OUTPUT_STV ||
	radeon_output->type == OUTPUT_CTV) {
      //	RADEONGetTVInfo(output);
    }

    if (radeon_output->DACType == DAC_TVDAC) {
	radeon_output->tv_on = FALSE;
	//	RADEONGetTVDacAdjInfo(output);
    }

}

Bool atombios_setup_outputs(ScrnInfoPtr pScrn, int num_vga, int num_dvi)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    xf86OutputPtr output;
    int i;

    for (i = 0 ; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    RADEONOutputPrivatePtr radeon_output = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
	    if (!radeon_output) {
		return FALSE;
	    }
	    radeon_output->MonType = MT_UNKNOWN;
	    radeon_output->ConnectorType = info->BiosConnector[i].ConnectorType;
	    radeon_output->DDCType = info->BiosConnector[i].DDCType;
	    radeon_output->gpio = info->BiosConnector[i].gpio;
	    if (info->IsAtomBios) {
		if (radeon_output->ConnectorType == CONNECTOR_DVI_D_ATOM)
		    radeon_output->DACType = DAC_NONE;
		else
		    radeon_output->DACType = info->BiosConnector[i].DACType;

		if (radeon_output->ConnectorType == CONNECTOR_VGA_ATOM)
		    radeon_output->TMDSType = TMDS_NONE;
		else
		    radeon_output->TMDSType = info->BiosConnector[i].TMDSType;
	    } else {
		if (radeon_output->ConnectorType == CONNECTOR_DVI_D)
		    radeon_output->DACType = DAC_NONE;
		else
		    radeon_output->DACType = info->BiosConnector[i].DACType;

		if (radeon_output->ConnectorType == CONNECTOR_CRT)
		    radeon_output->TMDSType = TMDS_NONE;
		else
		    radeon_output->TMDSType = info->BiosConnector[i].TMDSType;
	    }
	    RADEONSetOutputType(pScrn, radeon_output);
	    fprintf(stderr,"output type is %d\n", radeon_output->type);
	    if (info->IsAtomBios) {
		if ((info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_D_ATOM) ||
		    (info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_I_ATOM) ||
		    (info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_A_ATOM)) {
		    if (num_dvi > 1) {
			output = xf86OutputCreate(pScrn, &atombios_output_tmds_funcs, "DVI-1");
			num_dvi--;
		    } else {
		      output = xf86OutputCreate(pScrn, &atombios_output_tmds_funcs, "DVI-0");
		    }
		} else if (info->BiosConnector[i].ConnectorType == CONNECTOR_VGA_ATOM) {
		    if (num_vga > 1) {
			output = xf86OutputCreate(pScrn, &atombios_output_dac_funcs, "VGA-1");
			num_vga--;
		    } else {
 		        output = xf86OutputCreate(pScrn, &atombios_output_dac_funcs, "VGA-0");
		    }
		} else if (info->BiosConnector[i].ConnectorType != CONNECTOR_STV)
		    output = xf86OutputCreate(pScrn, &atombios_output_lfp_funcs, OutputType[radeon_output->type]);
	    } else {
		if ((info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_D) ||
		     (info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_I)) {
		    if (num_dvi > 1) {
			output = xf86OutputCreate(pScrn, &atombios_output_tmds_funcs, "DVI-1");
			num_dvi--;
		    } else {
			output = xf86OutputCreate(pScrn, &atombios_output_tmds_funcs, "DVI-0");
		    }
		} else if (info->BiosConnector[i].ConnectorType == CONNECTOR_CRT) {
		    if (num_vga > 1) {
			output = xf86OutputCreate(pScrn, &atombios_output_dac_funcs, "VGA-1");
			num_vga--;
		    } else {
			output = xf86OutputCreate(pScrn, &atombios_output_dac_funcs, "VGA-0");
		    }
		} else
		    output = xf86OutputCreate(pScrn, &atombios_output_lfp_funcs, OutputType[radeon_output->type]);
	    }

	    if (!output) {
		return FALSE;
	    }
	    output->driver_private = radeon_output;
	    output->possible_crtcs = 1;
	    /* crtc2 can drive LVDS, it just doesn't have RMX */
	    if (radeon_output->type != OUTPUT_LVDS)
		output->possible_crtcs |= 2;

	    /* we can clone the DACs, and probably TV-out, 
	       but I'm not sure it's worth the trouble */
	    output->possible_clones = 0;

	    atombios_init_connector(output);
	}
    }
    return TRUE;
}

static AtomBiosResult
atom_bios_dac_load_detect(atomBIOSHandlePtr atomBIOS, int dac)
{
  DAC_LOAD_DETECTION_PS_ALLOCATION dac_data;
  AtomBIOSArg data;
  unsigned char *space;

  dac_data.sDacload.usDeviceID = 0;
  dac_data.sDacload.ucDacType = 0;
  dac_data.sDacload.ucMisc = 0;

  data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC_LoadDetection);
  data.exec.dataSpace = (void *)&space;
  data.exec.pspace = &dac_data;
  
  if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {

    ErrorF("Dac detection success\n");
    return ATOM_SUCCESS ;
  }
  
  ErrorF("DAC detection failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static RADEONMonitorType atombios_dac_detect(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;
    AtomBiosResult ret;
    uint32_t bios_0_scratch;

    ret = atom_bios_dac_load_detect(info->atomBIOS, radeon_output->DACType);
    if (ret == ATOM_SUCCESS) {
	ErrorF("DAC connect %08X\n", INREG(0x10));
	bios_0_scratch = INREG(RADEON_BIOS_0_SCRATCH);
	
	if (radeon_output->DACType == DAC_PRIMARY) {
	    if (bios_0_scratch & ATOM_S0_CRT1_COLOR)
		MonType = MT_CRT;
	} else {
	    if (bios_0_scratch & ATOM_S0_CRT2_COLOR)
		MonType = MT_CRT;
	}
    }
    return MonType;
}

static RADEONMonitorType atombios_port_check_nonddc(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;
    uint32_t bios_0_scratch;
    int ret;
    if (radeon_output->type == OUTPUT_LVDS) {
	    MonType =  MT_LCD;
    }
#if 0
 else if (radeon_output->type == OUTPUT_DVI) {
	if (radeon_output->TMDSType == TMDS_INT) {
	    if (INREG(RADEON_FP_GEN_CNTL) & RADEON_FP_DETECT_SENSE)
		MonType = MT_DFP;
	} else if (radeon_output->TMDSType == TMDS_EXT) {
	    if (INREG(RADEON_FP2_GEN_CNTL) & RADEON_FP2_DETECT_SENSE)
		MonType = MT_DFP;
	}
    }
#endif
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Detected Monitor Type: %d\n", MonType);

    return MonType;

}


static RADEONMonitorType
atombios_display_ddc_connected(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned long DDCReg;
    RADEONMonitorType MonType = MT_NONE;
    xf86MonPtr* MonInfo = &output->MonInfo;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONDDCType DDCType = radeon_output->DDCType;
    int i, j;

    if (!IS_AVIVO_VARIANT) {
	ErrorF("AGD: DDCConnected\n");
	return RADEONDisplayDDCConnected(pScrn, output);
    }

    AVIVOI2CDoLock(output->scrn, 1);
    *MonInfo = xf86OutputGetEDID(output, radeon_output->pI2CBus);
    AVIVOI2CDoLock(output->scrn, 0);
    if (*MonInfo) {
	if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_LVDS_ATOM) ||
	    (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_PROPRIETARY)) {
	    MonType = MT_LCD;
	} else if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_D_ATOM) ||
		 (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_D)) {
	    MonType = MT_DFP;
	} else if (radeon_output->type == OUTPUT_DVI &&
		   ((*MonInfo)->rawData[0x14] & 0x80)) { /* if it's digital and DVI */
	    MonType = MT_DFP;
	} else {
	    MonType = MT_CRT;
	}
    } else MonType = MT_NONE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "DDC Type: %d[%04x], Detected Monitor Type: %d\n", DDCType, radeon_output->gpio, MonType);

    return MonType;
}

extern const char *ConnectorTypeNameATOM[];
extern const char *ConnectorTypeName[];

void atombios_connector_find_monitor(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    ErrorF("AGD: atom connector find monitor\n");

    if (radeon_output->MonType == MT_UNKNOWN) {
	radeon_output->MonType = atombios_display_ddc_connected(pScrn, output);
	if (!radeon_output->MonType) {
	    ErrorF("AGD: No DDC\n");
	    if (radeon_output->type == OUTPUT_LVDS || radeon_output->type == OUTPUT_DVI)
		radeon_output->MonType = atombios_port_check_nonddc(pScrn, output);
#if 0
	    if (!radeon_output->MonType) {
		if (radeon_output->DACType == DAC_PRIMARY) {
		    radeon_output->MonType = atombios_dac_detect(pScrn, output);
		} else if (radeon_output->DACType == DAC_TVDAC) {
		    radeon_output->MonType = atombios_dac_detect(pScrn, output);
		}
	    }
#endif
	    if (!radeon_output->MonType) {
		radeon_output->MonType = MT_NONE;
	    }
	}
    }
    /* update panel info for RMX */
    //    if (radeon_output->MonType == MT_LCD || radeon_output->MonType == MT_DFP)
    //	RADEONUpdatePanelSize(output);

    if (output->MonInfo) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on connector: %s ----------------------\n",
		   info->IsAtomBios ?
		   ConnectorTypeNameATOM[radeon_output->ConnectorType]:
		   ConnectorTypeName[radeon_output->ConnectorType]
		   );
	xf86PrintEDID( output->MonInfo );
    }
}
