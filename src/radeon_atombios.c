/*
 * Copyright 2007  Egbert Eich   <eich@novell.com>
 * Copyright 2007  Luc Verhaegen <lverhaegen@novell.com>
 * Copyright 2007  Matthias Hopf <mhopf@novell.com>
 * Copyright 2007  Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "xf86.h"
#include "xf86_OSproc.h"
#include "radeon.h"
#include "radeon_atombios.h"
#include "radeon_macros.h"

#include "xorg-server.h"
#if XSERVER_LIBPCIACCESS
#warning pciaccess defined
#endif

char *AtomBIOSQueryStr[] = {
    "Default Engine Clock",
    "Default Memory Clock",
    "Maximum Pixel ClockPLL Frequency Output",
    "Minimum Pixel ClockPLL Frequency Output",
    "Maximum Pixel ClockPLL Frequency Input",
    "Minimum Pixel ClockPLL Frequency Input",
    "Minimum Pixel Clock",
    "Reference Clock",
    "Start of VRAM area used by Firmware",
    "Framebuffer space used by Firmware (kb)",
    "TDMS Frequency",
    "PLL ChargePump",
    "PLL DutyCycle",
    "PLL VCO Gain",
    "PLL VoltageSwing"
};

char *AtomBIOSFuncStr[] = {
    "AtomBIOS Init",
    "AtomBIOS Teardown",
    "AtomBIOS Exec",
    "AtomBIOS Set FB Space"
};

#define DEBUGP(x) {x;}
#define LOG_DEBUG 7

#ifdef ATOM_BIOS
# define LOG_CAIL LOG_DEBUG + 1

#ifdef ATOM_BIOS_PARSER
static void
CailDebug(int scrnIndex, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    xf86VDrvMsgVerb(scrnIndex, X_INFO, LOG_CAIL, format, ap);
    va_end(ap);
}
#endif

# define CAILFUNC(ptr) \
  CailDebug(((atomBIOSHandlePtr)(ptr))->scrnIndex, "CAIL: %s\n", __func__)


enum {
    legacyBIOSLocation = 0xC0000,
    legacyBIOSMax = 0x10000
};

static int
rhdAnalyzeCommonHdr(ATOM_COMMON_TABLE_HEADER *hdr)
{
    if (hdr->usStructureSize == 0xaa55)
        return FALSE;

    return TRUE;
}

static int
rhdAnalyzeRomHdr(unsigned char *rombase,
              ATOM_ROM_HEADER *hdr,
		 int *data_offset, int *command_offset)
{
    if (rhdAnalyzeCommonHdr(&hdr->sHeader) == -1) {
        return FALSE;
    }
    xf86ErrorF("\tSubsysemVendorID: 0x%4.4x SubsystemID: 0x%4.4x\n",
               hdr->usSubsystemVendorID,hdr->usSubsystemID);
    xf86ErrorF("\tIOBaseAddress: 0x%4.4x\n",hdr->usIoBaseAddress);
    xf86ErrorFVerb(3,"\tFilename: %s\n",rombase + hdr->usConfigFilenameOffset);
    xf86ErrorFVerb(3,"\tBIOS Bootup Message: %s\n",
		   rombase + hdr->usBIOS_BootupMessageOffset);

    *data_offset = hdr->usMasterDataTableOffset;
    *command_offset = hdr->usMasterCommandTableOffset;

    return TRUE;
}

static int
rhdAnalyzeRomDataTable(unsigned char *base, int offset,
                    void *ptr,short *size)
{
    ATOM_COMMON_TABLE_HEADER *table = (ATOM_COMMON_TABLE_HEADER *)
        (base + offset);

   if (!*size || rhdAnalyzeCommonHdr(table) == -1) {
       if (*size) *size -= 2;
       *(void **)ptr = NULL;
       return FALSE;
   }
   *size -= 2;
   *(void **)ptr = (void *)(table);
   return TRUE;
}

static Bool
rhdGetAtomBiosTableRevisionAndSize(ATOM_COMMON_TABLE_HEADER *hdr,
                                   CARD8 *contentRev,
                                   CARD8 *formatRev,
                                   short *size)
{
    if (!hdr)
        return FALSE;

    if (contentRev) *contentRev = hdr->ucTableContentRevision;
    if (formatRev) *formatRev = hdr->ucTableFormatRevision;
    if (size) *size = (short)hdr->usStructureSize
                   - sizeof(ATOM_COMMON_TABLE_HEADER);
    return TRUE;
}

static Bool
rhdAnalyzeMasterDataTable(unsigned char *base,
                       ATOM_MASTER_DATA_TABLE *table,
                       atomDataTablesPtr data)
{
    ATOM_MASTER_LIST_OF_DATA_TABLES *data_table =
        &table->ListOfDataTables;
    short size;

    if (!rhdAnalyzeCommonHdr(&table->sHeader))
        return FALSE;
    if (!rhdGetAtomBiosTableRevisionAndSize(&table->sHeader,NULL,NULL,&size))
        return FALSE;
# define SET_DATA_TABLE(x) {\
   rhdAnalyzeRomDataTable(base,data_table->x,(void *)(&(data->x)),&size); \
    }

# define SET_DATA_TABLE_VERS(x) {\
   rhdAnalyzeRomDataTable(base,data_table->x,&(data->x.base),&size); \
    }

    SET_DATA_TABLE(UtilityPipeLine);
    SET_DATA_TABLE(MultimediaCapabilityInfo);
    SET_DATA_TABLE(MultimediaConfigInfo);
    SET_DATA_TABLE(StandardVESA_Timing);
    SET_DATA_TABLE_VERS(FirmwareInfo);
    SET_DATA_TABLE(DAC_Info);
    SET_DATA_TABLE_VERS(LVDS_Info);
    SET_DATA_TABLE(TMDS_Info);
    SET_DATA_TABLE(AnalogTV_Info);
    SET_DATA_TABLE_VERS(SupportedDevicesInfo);
    SET_DATA_TABLE(GPIO_I2C_Info);
    SET_DATA_TABLE(VRAM_UsageByFirmware);
    SET_DATA_TABLE(GPIO_Pin_LUT);
    SET_DATA_TABLE(VESA_ToInternalModeLUT);
    SET_DATA_TABLE_VERS(ComponentVideoInfo);
    SET_DATA_TABLE(PowerPlayInfo);
    SET_DATA_TABLE(CompassionateData);
    SET_DATA_TABLE(SaveRestoreInfo);
    SET_DATA_TABLE(PPLL_SS_Info);
    SET_DATA_TABLE(OemInfo);
    SET_DATA_TABLE(XTMDS_Info);
    SET_DATA_TABLE(MclkSS_Info);
    SET_DATA_TABLE(Object_Header);
    SET_DATA_TABLE(IndirectIOAccess);
    SET_DATA_TABLE(MC_InitParameter);
    SET_DATA_TABLE(ASIC_VDDC_Info);
    SET_DATA_TABLE(ASIC_InternalSS_Info);
    SET_DATA_TABLE(TV_VideoMode);
    SET_DATA_TABLE_VERS(VRAM_Info);
    SET_DATA_TABLE(MemoryTrainingInfo);
    SET_DATA_TABLE_VERS(IntegratedSystemInfo);
    SET_DATA_TABLE(ASIC_ProfilingInfo);
    SET_DATA_TABLE(VoltageObjectInfo);
    SET_DATA_TABLE(PowerSourceInfo);
# undef SET_DATA_TABLE

    return TRUE;
}

Bool
rhdGetAtombiosDataTable(int scrnIndex, unsigned char *base, int *cmd_offset,
                        atomDataTables *atomDataPtr)
{
    int  data_offset;
    unsigned short atom_romhdr_off =  *(unsigned short*)
        (base + OFFSET_TO_POINTER_TO_ATOM_ROM_HEADER);
    ATOM_ROM_HEADER *atom_rom_hdr =
        (ATOM_ROM_HEADER *)(base + atom_romhdr_off);

    //    RHDFUNCI(scrnIndex)

    if (memcmp("ATOM",&atom_rom_hdr->uaFirmWareSignature,4)) {
        xf86DrvMsg(scrnIndex,X_ERROR,"No AtomBios signature found\n");
        return FALSE;
    }
    xf86DrvMsg(scrnIndex, X_INFO, "ATOM BIOS Rom: \n");
    if (!rhdAnalyzeRomHdr(base, atom_rom_hdr, &data_offset, cmd_offset)) {
        xf86DrvMsg(scrnIndex, X_ERROR, "RomHeader invalid\n");
        return FALSE;
    }
    if (!rhdAnalyzeMasterDataTable(base, (ATOM_MASTER_DATA_TABLE *)
                              (base + data_offset),
                                atomDataPtr)) {
        xf86DrvMsg(scrnIndex, X_ERROR, "ROM Master Table invalid\n");
        return FALSE;
    }
    return TRUE;
}

static Bool
rhdBIOSGetFbBaseAndSize(int scrnIndex, atomBIOSHandlePtr handle, unsigned int *base, unsigned int *size)
{
    AtomBIOSArg data;
    if (RHDAtomBIOSFunc(scrnIndex, handle, GET_FW_FB_SIZE, &data)
	== ATOM_SUCCESS) {
	if (data.val == 0) {
	    xf86DrvMsg(scrnIndex, X_WARNING, "%s: AtomBIOS specified VRAM "
		       "scratch space size invalid\n", __func__);
	    return FALSE;
	}
	*size = (int)data.val;
    } else
	return FALSE;
    if (RHDAtomBIOSFunc(scrnIndex, handle, GET_FW_FB_START, &data)
	== ATOM_SUCCESS) {
	if (data.val == 0)
	    return FALSE;
	*base = (int)data.val;
    }
    return TRUE;
}

/*
 * Uses videoRam form ScrnInfoRec.
 */
static Bool
rhdAtomBIOSAllocateFbScratch(int scrnIndex, atomBIOSHandlePtr handle,
		     unsigned *start, unsigned int *size)
{
    unsigned int fb_base = 0;
    unsigned int fb_size = 0;
    handle->scratchBase = NULL;
    handle->fbBase = 0;

    if (rhdBIOSGetFbBaseAndSize(scrnIndex, handle, &fb_base, &fb_size)) {
	xf86DrvMsg(scrnIndex, X_INFO, "AtomBIOS requests %ikB"
		   " of VRAM scratch space\n",fb_size);
	fb_size *= 1024; /* convert to bytes */
	xf86DrvMsg(scrnIndex, X_INFO, "AtomBIOS VRAM scratch base: 0x%x\n",
		   fb_base);
    } else {
	    fb_size = 20 * 1024;
	    xf86DrvMsg(scrnIndex, X_INFO, " default to: %i\n",fb_size);
    }
    if (fb_base && fb_size && *size) {
	/* 4k align */
	fb_size = (fb_size & ~(CARD32)0xfff) + ((fb_size & 0xfff) ? 1 : 0);
	if ((fb_base + fb_size) > (*start + *size)) {
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "%s: FW FB scratch area %i (size: %i)"
		       " extends beyond available framebuffer size %i\n",
		       __func__, fb_base, fb_size, *size);
	} else if ((fb_base + fb_size) < (*start + *size)) {
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "%s: FW FB scratch area not located "
		       "at the end of VRAM. Scratch End: "
		       "0x%x VRAM End: 0x%x\n", __func__,
		       (unsigned int)(fb_base + fb_size),
		       *size);
	} else if (fb_base < *start) {
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "%s: FW FB scratch area extends below "
		       "the base of the free VRAM: 0x%x Base: 0x%x\n",
		       __func__, (unsigned int)(fb_base), *start);
	} else {
	    *size -= fb_size;
	    handle->fbBase = fb_base;
	    return TRUE;
	}
    }

    if (!handle->fbBase) {
	xf86DrvMsg(scrnIndex, X_INFO,
		   "Cannot get VRAM scratch space. "
		   "Allocating in main memory instead\n");
	handle->scratchBase = xcalloc(fb_size,1);
	return TRUE;
    }
    return FALSE;
}

# ifdef ATOM_BIOS_PARSER
static Bool
rhdASICInit(atomBIOSHandlePtr handle)
{
    ASIC_INIT_PS_ALLOCATION asicInit;
    AtomBIOSArg data;

    RHDAtomBIOSFunc(handle->scrnIndex, handle,
		    GET_DEFAULT_ENGINE_CLOCK,
		    &data);
    asicInit.sASICInitClocks.ulDefaultEngineClock = data.val;  /* in 10 Khz */
    RHDAtomBIOSFunc(handle->scrnIndex, handle,
		    GET_DEFAULT_MEMORY_CLOCK,
		    &data);
    asicInit.sASICInitClocks.ulDefaultMemoryClock = data.val;  /* in 10 Khz */
    data.exec.dataSpace = NULL;
    data.exec.index = 0x0;
    data.exec.pspace = &asicInit;
    xf86DrvMsg(handle->scrnIndex, X_INFO, "Calling ASIC Init\n");
    if (RHDAtomBIOSFunc(handle->scrnIndex, handle,
			ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	xf86DrvMsg(handle->scrnIndex, X_INFO, "ASIC_INIT Successful\n");
	return TRUE;
    }
    xf86DrvMsg(handle->scrnIndex, X_INFO, "ASIC_INIT Failed\n");
    return FALSE;
}
# endif

static atomBIOSHandlePtr
rhdInitAtomBIOS(int scrnIndex)
{
    RADEONInfoPtr  info   = RADEONPTR(xf86Screens[scrnIndex]);
    unsigned char *ptr = info->VBIOS;
    atomDataTablesPtr atomDataPtr;
    atomBIOSHandlePtr handle = NULL;
    unsigned int dummy;
    int cmd_offset;

    if (!(atomDataPtr = xcalloc(sizeof(atomDataTables),1))) {
	xf86DrvMsg(scrnIndex,X_ERROR,"Cannot allocate memory for "
		   "ATOM BIOS data tabes\n");
	goto error;
    }
    if (!rhdGetAtombiosDataTable(scrnIndex, ptr, &cmd_offset, atomDataPtr))
	goto error1;
    if (!(handle = xcalloc(sizeof(atomBIOSHandle),1))) {
	xf86DrvMsg(scrnIndex,X_ERROR,"Cannot allocate memory\n");
	goto error1;
    }
    handle->cmd_offset = cmd_offset;
    handle->BIOSBase = ptr;
    handle->atomDataPtr = atomDataPtr;
    handle->scrnIndex = scrnIndex;
#if XSERVER_LIBPCIACCESS
    handle->device = info->PciInfo;
#else
    handle->PciTag = info->PciTag;
#endif

#if ATOM_BIOS_PARSER
    /* Try to find out if BIOS has been posted (either by system or int10 */
    if (!rhdBIOSGetFbBaseAndSize(scrnIndex, handle, &dummy, &dummy)) {
	/* run AsicInit */
	if (!rhdASICInit(handle))
	    xf86DrvMsg(scrnIndex, X_WARNING,
		       "%s: AsicInit failed. Won't be able to obtain in VRAM "
		       "FB scratch space\n",__func__);
    }
#endif
    return handle;

 error1:
    xfree(atomDataPtr);
 error:
    xfree(ptr);
    return NULL;
}

void
rhdTearDownAtomBIOS(int scrnIndex, atomBIOSHandlePtr handle)
{
  //    RHDFUNCI(scrnIndex);

    xfree(handle->BIOSBase);
    xfree(handle->atomDataPtr);
    xfree(handle);
}
AtomBiosResult
rhdAtomBIOSVramInfoQuery(int scrnIndex, atomBIOSHandlePtr handle, AtomBiosFunc func,
			     CARD32 *val)
{
    atomDataTablesPtr atomDataPtr;

    //    RHDFUNCI(scrnIndex);

    atomDataPtr = handle->atomDataPtr;

    switch (func) {
	case GET_FW_FB_START:
	    *val = atomDataPtr->VRAM_UsageByFirmware
		->asFirmwareVramReserveInfo[0].ulStartAddrUsedByFirmware;
	    break;
	case GET_FW_FB_SIZE:
	    *val = atomDataPtr->VRAM_UsageByFirmware
		->asFirmwareVramReserveInfo[0].usFirmwareUseInKb;
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
    }
    return ATOM_SUCCESS;
}

AtomBiosResult
rhdAtomBIOSTmdsInfoQuery(int scrnIndex, atomBIOSHandlePtr handle,
			 AtomBiosFunc func, int index, CARD32 *val)
{
    atomDataTablesPtr atomDataPtr;

    atomDataPtr = handle->atomDataPtr;
    if (!rhdGetAtomBiosTableRevisionAndSize(
	    (ATOM_COMMON_TABLE_HEADER *)(atomDataPtr->FirmwareInfo.base),
	    NULL,NULL,NULL)) {
	return ATOM_FAILED;
    }

    //    RHDFUNCI(scrnIndex);

    switch (func) {
	case ATOM_TMDS_FREQUENCY:
	    *val = atomDataPtr->TMDS_Info->asMiscInfo[index].usFrequency;
	    break;
	case ATOM_TMDS_PLL_CHARGE_PUMP:
	    *val = atomDataPtr->TMDS_Info->asMiscInfo[index].ucPLL_ChargePump;
	    break;
	case ATOM_TMDS_PLL_DUTY_CYCLE:
	    *val = atomDataPtr->TMDS_Info->asMiscInfo[index].ucPLL_DutyCycle;
	    break;
	case ATOM_TMDS_PLL_VCO_GAIN:
	    *val = atomDataPtr->TMDS_Info->asMiscInfo[index].ucPLL_VCO_Gain;
	    break;
	case ATOM_TMDS_PLL_VOLTAGE_SWING:
	    *val = atomDataPtr->TMDS_Info->asMiscInfo[index].ucPLL_VoltageSwing;
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
    }
    return ATOM_SUCCESS;
}

AtomBiosResult
rhdAtomBIOSFirmwareInfoQuery(int scrnIndex, atomBIOSHandlePtr handle,
			     AtomBiosFunc func, CARD32 *val)
{
    atomDataTablesPtr atomDataPtr;
    CARD8 crev, frev;

    //    RHDFUNCI(scrnIndex);

    atomDataPtr = handle->atomDataPtr;
    if (!rhdGetAtomBiosTableRevisionAndSize(
	    (ATOM_COMMON_TABLE_HEADER *)(atomDataPtr->FirmwareInfo.base),
	    &crev,&frev,NULL)) {
	return ATOM_FAILED;
    }
    switch (crev) {
	case 1:
	    switch (func) {
		case GET_DEFAULT_ENGINE_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->ulDefaultEngineClock;
		    break;
		case GET_DEFAULT_MEMORY_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->ulDefaultMemoryClock;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->ulMaxPixelClockPLL_Output;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->usMinPixelClockPLL_Output;
		case GET_MAX_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->usMaxPixelClockPLL_Input;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->usMinPixelClockPLL_Input;
		    break;
		case GET_MAX_PIXEL_CLK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->usMaxPixelClock;
		    break;
		case GET_REF_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo->usReferenceClock;
		    break;
		default:
		    return ATOM_NOT_IMPLEMENTED;
	    }
	case 2:
	    switch (func) {
		case GET_DEFAULT_ENGINE_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->ulDefaultEngineClock;
		    break;
		case GET_DEFAULT_MEMORY_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->ulDefaultMemoryClock;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->ulMaxPixelClockPLL_Output;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->usMinPixelClockPLL_Output;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->usMaxPixelClockPLL_Input;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->usMinPixelClockPLL_Input;
		    break;
		case GET_MAX_PIXEL_CLK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->usMaxPixelClock;
		    break;
		case GET_REF_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_2->usReferenceClock;
		    break;
		default:
		    return ATOM_NOT_IMPLEMENTED;
	    }
	    break;
	case 3:
	    switch (func) {
		case GET_DEFAULT_ENGINE_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->ulDefaultEngineClock;
		    break;
		case GET_DEFAULT_MEMORY_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->ulDefaultMemoryClock;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->ulMaxPixelClockPLL_Output;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->usMinPixelClockPLL_Output;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->usMaxPixelClockPLL_Input;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->usMinPixelClockPLL_Input;
		    break;
		case GET_MAX_PIXEL_CLK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->usMaxPixelClock;
		    break;
		case GET_REF_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_3->usReferenceClock;
		    break;
		default:
		    return ATOM_NOT_IMPLEMENTED;
	    }
	    break;
	case 4:
	    switch (func) {
		case GET_DEFAULT_ENGINE_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->ulDefaultEngineClock;
		    break;
		case GET_DEFAULT_MEMORY_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->ulDefaultMemoryClock;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->usMaxPixelClockPLL_Input;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_INPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->usMinPixelClockPLL_Input;
		    break;
		case GET_MAX_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->ulMaxPixelClockPLL_Output;
		    break;
		case GET_MIN_PIXEL_CLOCK_PLL_OUTPUT:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->usMinPixelClockPLL_Output;
		    break;
		case GET_MAX_PIXEL_CLK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->usMaxPixelClock;
		    break;
		case GET_REF_CLOCK:
		    *val = atomDataPtr->FirmwareInfo
			.FirmwareInfo_V_1_4->usReferenceClock;
		    break;
		default:
		    return ATOM_NOT_IMPLEMENTED;
	    }
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
    }
    return ATOM_SUCCESS;
}

# ifdef ATOM_BIOS_PARSER
static Bool
rhdAtomExec (atomBIOSHandlePtr handle, int index, void *pspace,  pointer *dataSpace)
{
    RADEONInfoPtr info = RADEONPTR (xf86Screens[handle->scrnIndex]);
    Bool ret = FALSE;
    char *msg;

    //    RHDFUNCI(handle->scrnIndex);
    if (dataSpace) {
	if (!handle->fbBase && !handle->scratchBase)
	    return FALSE;
	if (handle->fbBase) {
	    if (!info->FB) {
		xf86DrvMsg(handle->scrnIndex, X_ERROR, "%s: "
			   "Cannot exec AtomBIOS: framebuffer not mapped\n",
			   __func__);
		return FALSE;
	    }
	    *dataSpace = (CARD8*)info->FB + handle->fbBase;
	} else
	    *dataSpace = (CARD8*)handle->scratchBase;
    }
    ret = ParseTableWrapper(pspace, index, handle,
			    handle->BIOSBase,
			    &msg);
    if (!ret)
	xf86DrvMsg(handle->scrnIndex, X_ERROR, "%s\n",msg);
    else
	xf86DrvMsgVerb(handle->scrnIndex, X_INFO, 5, "%s\n",msg);

    return (ret) ? TRUE : FALSE;
}
# endif

AtomBiosResult
RHDAtomBIOSFunc(int scrnIndex, atomBIOSHandlePtr handle, AtomBiosFunc func,
		AtomBIOSArgPtr data)
{
    AtomBiosResult ret = ATOM_NOT_IMPLEMENTED;
    CARD32 val;

# define do_return(x) { \
        if (func < sizeof(AtomBIOSFuncStr)) \
	  xf86DrvMsgVerb(scrnIndex, (x == ATOM_SUCCESS) ? 7 : 1,	\
		           (x == ATOM_SUCCESS) ? X_INFO : X_WARNING,	\
		           "Call to %s %s\n", AtomBIOSFuncStr[func], \
		           (x == ATOM_SUCCESS) ? "succeeded" : "FAILED"); \
	    return (x); \
    }
    assert (sizeof(AtomBIOSQueryStr) == (FUNC_END - ATOM_QUERY_FUNCS + 1));

    if (func == ATOMBIOS_INIT) {
	if (!(data->atomp = rhdInitAtomBIOS(scrnIndex)))
	    do_return(ATOM_FAILED);
	do_return(ATOM_SUCCESS);
    }
    if (!handle)
	do_return(ATOM_FAILED);
    if (func == ATOMBIOS_ALLOCATE_FB_SCRATCH) {
        if (rhdAtomBIOSAllocateFbScratch( scrnIndex, handle, &data->fb.start, &data->fb.size)) {
	    do_return(ATOM_SUCCESS);
	} else {
	    do_return(ATOM_FAILED);
	}
    }
    if (func <= ATOMBIOS_TEARDOWN) {
	rhdTearDownAtomBIOS(scrnIndex, handle);
	do_return(ATOM_SUCCESS);
    }
# ifdef ATOM_BIOS_PARSER
    if (func == ATOMBIOS_EXEC) {
	if (!rhdAtomExec(handle, data->exec.index,
			 data->exec.pspace, data->exec.dataSpace)) {
	    do_return(ATOM_FAILED);
	} else {
	    do_return(ATOM_SUCCESS);
	}
    } else
# endif
	if (func >= ATOM_QUERY_FUNCS && func < ATOM_VRAM_QUERIES) {
	ret = rhdAtomBIOSFirmwareInfoQuery(scrnIndex, handle, func, &val);
	data->val = val;
    } else if (func >= ATOM_VRAM_QUERIES && func < FUNC_END) {
	ret = rhdAtomBIOSVramInfoQuery(scrnIndex, handle, func, &val);
	data->val = val;
    } else {
	xf86DrvMsg(scrnIndex,X_INFO,"%s: Received unknown query\n",__func__);
	return ATOM_NOT_IMPLEMENTED;
    }
    if (ret == ATOM_SUCCESS)
	xf86DrvMsg(scrnIndex,X_INFO,"%s: %i 0x%08x\n",
		   AtomBIOSQueryStr[func - ATOM_QUERY_FUNCS], (unsigned int)val, (unsigned int)val);
    else
	xf86DrvMsg(scrnIndex,X_INFO,"Query for %s: %s\n",
		   AtomBIOSQueryStr[func - ATOM_QUERY_FUNCS],
		   ret == ATOM_FAILED ? "failed" : "not implemented");
    return ret;

}

# ifdef ATOM_BIOS_PARSER
VOID*
CailAllocateMemory(VOID *CAIL,UINT16 size)
{
    CAILFUNC(CAIL);

    return malloc(size);
}

VOID
CailReleaseMemory(VOID *CAIL, VOID *addr)
{
    CAILFUNC(CAIL);

    free(addr);
}

VOID
CailDelayMicroSeconds(VOID *CAIL, UINT32 delay)
{
    CAILFUNC(CAIL);

    usleep(delay);

    //    DEBUGP(xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,X_INFO,"Delay %i usec\n",delay));
}

UINT32
CailReadATIRegister(VOID* CAIL, UINT32 index)
{
    UINT32 ret;
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CAILFUNC(CAIL);

    ret  =  INREG(index << 2);
    DEBUGP(ErrorF("%s(%x) = %x\n",__func__,index << 2,ret));
    return ret;
}

VOID
CailWriteATIRegister(VOID *CAIL, UINT32 index, UINT32 data)
{
    CAILFUNC(CAIL);
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(index << 2,data);
    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,index << 2,data));
}

UINT32
CailReadFBData(VOID* CAIL, UINT32 index)
{
    UINT32 ret;
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    CAILFUNC(CAIL);

    if (((atomBIOSHandlePtr)CAIL)->fbBase) {
	CARD8 *FBBase = (CARD8*)info->FB;
	ret =  *((CARD32*)(FBBase + (((atomBIOSHandlePtr)CAIL)->fbBase) + index));
	DEBUGP(ErrorF("%s(%x) = %x\n",__func__,index,ret));
    } else if (((atomBIOSHandlePtr)CAIL)->scratchBase) {
	ret = *(CARD32*)((CARD8*)(((atomBIOSHandlePtr)CAIL)->scratchBase) + index);
	DEBUGP(ErrorF("%s(%x) = %x\n",__func__,index,ret));
    } else {
	xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,X_ERROR,
		   "%s: no fbbase set\n",__func__);
	return 0;
    }
    return ret;
}

VOID
CailWriteFBData(VOID *CAIL, UINT32 index, UINT32 data)
{
    CAILFUNC(CAIL);

    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,index,data));
    if (((atomBIOSHandlePtr)CAIL)->fbBase) {
	CARD8 *FBBase = (CARD8*)
	    RADEONPTR(xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex])->FB;
	*((CARD32*)(FBBase + (((atomBIOSHandlePtr)CAIL)->fbBase) + index)) = data;
    } else if (((atomBIOSHandlePtr)CAIL)->scratchBase) {
	*(CARD32*)((CARD8*)(((atomBIOSHandlePtr)CAIL)->scratchBase) + index) = data;
    } else
	xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,X_ERROR,
		   "%s: no fbbase set\n",__func__);
}

ULONG
CailReadMC(VOID *CAIL, ULONG Address)
{
    ULONG ret;
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    CAILFUNC(CAIL);

    ret = INMC(pScrn, Address);
    DEBUGP(ErrorF("%s(%x) = %x\n",__func__,Address,ret));
    return ret;
}

VOID
CailWriteMC(VOID *CAIL, ULONG Address, ULONG data)
{
    CAILFUNC(CAIL);
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,Address,data));
    OUTMC(pScrn, Address, data);
}

VOID
CailReadPCIConfigData(VOID*CAIL, VOID* ret, UINT32 index,UINT16 size)
{
#if !XSERVER_LIBPCIACCESS
    PCITAG tag = ((atomBIOSHandlePtr)CAIL)->PciTag;

    CAILFUNC(CAIL);

    switch (size) {
	case 8:
	    *(CARD8*)ret = pciReadByte(tag,index << 2);
	    break;
	case 16:
	    *(CARD16*)ret = pciReadWord(tag,index << 2);
	    break;
	case 32:
	    *(CARD32*)ret = pciReadLong(tag,index << 2);
	    break;
	default:
	xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,
		   X_ERROR,"%s: Unsupported size: %i\n",
		   __func__,(int)size);
	return;
	    break;
    }
#else
    struct pci_device *device = ((atomBIOSHandlePtr)CAIL)->device;

    CAILFUNC(CAIL);

    switch (size) {
	case 8:
	    pci_device_cfg_read_u8(device, (CARD8*)ret, index << 2);
	    break;
	case 16:
	    pci_device_cfg_read_u16(device, (CARD16*)ret, index << 2);
	    break;
	case 32:
	    pci_device_cfg_read_u32(device, (uint32_t*)ret, index << 2);
	    break;
	default:
	xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,
		   X_ERROR,"%s: Unsupported size: %i\n",
		   __func__,(int)size);
	return;
	    break;
    }
    DEBUGP(ErrorF("%s(%x) = %x\n",__func__,index,*(unsigned int*)ret));
#endif
}

VOID
CailWritePCIConfigData(VOID*CAIL,VOID*src,UINT32 index,UINT16 size)
{
#if !XSERVER_LIBPCIACCESS
    PCITAG tag = ((atomBIOSHandlePtr)CAIL)->PciTag;

    CAILFUNC(CAIL);
    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,index,(*(unsigned int*)src)));
    switch (size) {
	case 8:
	    pciWriteByte(tag,index << 2,*(CARD8*)src);
	    break;
	case 16:
	    pciWriteWord(tag,index << 2,*(CARD16*)src);
	    break;
	case 32:
	    pciWriteLong(tag,index << 2,*(CARD32*)src);
	    break;
	default:
	    xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,X_ERROR,
		       "%s: Unsupported size: %i\n",__func__,(int)size);
	    break;
    }
#else
    struct pci_device *device = ((atomBIOSHandlePtr)CAIL)->device;

    CAILFUNC(CAIL);
    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,index,(*(unsigned int*)src)));
    switch (size) {
	case 8:
	    pci_device_cfg_write_u8(device,index << 2,*(CARD8*)src);
	    break;
	case 16:
	    pci_device_cfg_write_u16(device,index << 2,*(uint16_t *)src);
	    break;
	case 32:
	    pci_device_cfg_write_u32(device,index << 2,*(uint32_t *)src);
	    break;
	default:
	    xf86DrvMsg(((atomBIOSHandlePtr)CAIL)->scrnIndex,X_ERROR,
		       "%s: Unsupported size: %i\n",__func__,(int)size);
	    break;
    }
#endif
}

ULONG
CailReadPLL(VOID *CAIL, ULONG Address)
{
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    ULONG ret;

    CAILFUNC(CAIL);

    ret = RADEONINPLL(pScrn, Address);
    DEBUGP(ErrorF("%s(%x) = %x\n",__func__,Address,ret));
    return ret;
}

VOID
CailWritePLL(VOID *CAIL, ULONG Address,ULONG Data)
{
    ScrnInfoPtr pScrn = xf86Screens[((atomBIOSHandlePtr)CAIL)->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CAILFUNC(CAIL);

    DEBUGP(ErrorF("%s(%x,%x)\n",__func__,Address,Data));
    RADEONOUTPLL(pScrn, Address, Data);
}

void
rhdTestAtomBIOS(atomBIOSHandlePtr atomBIOS)
{
    READ_EDID_FROM_HW_I2C_DATA_PARAMETERS i2cData;
    AtomBIOSArg data;
    int i;
    unsigned char *space;

    i2cData.usPrescale = 0x7fff;
    i2cData.usVRAMAddress = 0;
    i2cData.usStatus = 128;
    i2cData.ucSlaveAddr = 0xA0;

    data.exec.dataSpace = (void*)&space;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, ReadEDIDFromHWAssistedI2C);
    data.exec.pspace = &i2cData;

    for (i = 0; i < 4; i++) {
	i2cData.ucLineNumber = i;
	if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	    int j;
	    CARD8 chksum = 0;
	    xf86DrvMsg(atomBIOS->scrnIndex, X_INFO,"%s: I2C channel %i STATUS: %x\n",
		       __func__,i,i2cData.usStatus);
	    /* read good ? */
	    if ((i2cData.usStatus >> 8) == HW_ASSISTED_I2C_STATUS_SUCCESS) {
		/* checksum good? */
	      ErrorF("i2c data ustatus good %04X\n", i2cData.usStatus);
		if (!(i2cData.usStatus & 0xff)) {
#if 0
		    RhdDebugDump(atomBIOS->scrnIndex, space, 128);
#endif
		    for (j = 0; j < 128; j++)
			chksum += space[i];
		    xf86DrvMsg(atomBIOS->scrnIndex, X_INFO, "DDC Checksum: %i\n",chksum);
		}
	    }
	}
    }
}
# endif

#else /* ATOM_BIOS */

AtomBiosResult
RHDAtomBIOSFunc(int scrnIndex, atomBIOSHandlePtr handle, AtomBiosFunc func,
		AtomBIOSArgPtr data)
{
    assert (sizeof(AtomBIOSQueryStr) == (FUNC_END - ATOM_QUERY_FUNCS + 1));

    if (func < ATOM_QUERY_FUNCS) {
	if (func >= 0 && func < sizeof(AtomBIOSFuncStr))
	    xf86DrvMsgVerb(scrnIndex, 5, X_WARNING,
			   "AtomBIOS support not available, cannot execute %s\n",
			   AtomBIOSFuncStr[func]);
	else
	    xf86DrvMsg(scrnIndex, X_ERROR,"Invalid AtomBIOS func %x\n",func);
    } else {

	if (func < FUNC_END)
	    xf86DrvMsgVerb(scrnIndex, 5, X_WARNING,
			    "AtomBIOS not available, cannot get %s\n",
			   AtomBIOSQueryStr[func - ATOM_QUERY_FUNCS]);
	else
	    xf86DrvMsg(scrnIndex, X_ERROR, "Invalid AtomBIOS query %x\n",func);
    }
    return ATOM_NOT_IMPLEMENTED;
}

#endif /* ATOM_BIOS */


AtomBiosResult
RADEONAtomBIOSSetCrtcSource(atomBIOSHandlePtr atomBIOS, int crtc, int output_mask)
{
    SELECT_CRTC_SOURCE_PARAMETERS crtc_data;
    AtomBIOSArg data;
    unsigned char *space;

    crtc_data.ucCRTC = crtc;
    crtc_data.ucDevice = ATOM_DEVICE_CRT1_SUPPORT;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, EnableCRTC);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_data;
    
    if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC source success\n");
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Set CRTC source failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

void
atombios_get_command_table_version(atomBIOSHandlePtr atomBIOS, int index, int *major, int *minor)
{
    ATOM_MASTER_COMMAND_TABLE *cmd_table = atomBIOS->BIOSBase + atomBIOS->cmd_offset;
    ATOM_MASTER_LIST_OF_COMMAND_TABLES *table_start;
    ATOM_COMMON_ROM_COMMAND_TABLE_HEADER *table_hdr;

    unsigned short *ptr;
    unsigned short offset;

    table_start = &cmd_table->ListOfCommandTables;

    offset  = *(((unsigned short *)table_start) + index);
    
    table_hdr = atomBIOS->BIOSBase + offset;

    *major = table_hdr->CommonHeader.ucTableFormatRevision;
    *minor = table_hdr->CommonHeader.ucTableContentRevision;
}
