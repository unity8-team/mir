/*************************************************************************************
 * $Id$
 * 
 * Created by Bogdan D. bogdand@users.sourceforge.net 
 * License: GPL
 *
 * $Log$
 * Revision 1.1  2005/04/17 23:06:17  bogdand
 * Added the RageTheatre200 video demodulator support
 *
 *
 ************************************************************************************/

#ifndef __THEATRE200_H__
#define __THEATRE200_H__

#include "theatre.h"

#define DEFAULT_MICROC_PATH "/usr/X11R6/lib/modules/multimedia/rt2_pmem.bin"
#define DEFAULT_MICROC_TYPE "BINARY"

/* #define ENABLE_DEBUG 1 */

#ifdef ENABLE_DEBUG
#define ERROR(str...) xf86DrvMsg(screen, X_ERROR, ##str)
#define DEBUG(str...) xf86DrvMsg(screen, X_INFO, ##str) 
#else
#define ERROR(fmt,str...)
#define DEBUG(fmt,str...)
#endif


#define DSP_OK						0x21
#define DSP_INVALID_PARAMETER		0x22
#define DSP_MISSING_PARAMETER		0x23
#define DSP_UNKNOWN_COMMAND			0x24
#define DSP_UNSUCCESS				0x25
#define DSP_BUSY					0x26
#define DSP_RESET_REQUIRED			0x27
#define DSP_UNKNOWN_RESULT			0x28
#define DSP_CRC_ERROR				0x29
#define DSP_AUDIO_GAIN_ADJ_FAIL		0x2a
#define DSP_AUDIO_GAIN_CHK_ERROR	0x2b
#define DSP_WARNING					0x2c
#define DSP_POWERDOWN_MODE			0x2d

#define RT200_NTSC_M				0x01
#define RT200_NTSC_433				0x03
#define RT200_NTSC_J				0x04
#define RT200_PAL_B					0x05
#define RT200_PAL_D					0x06
#define RT200_PAL_G					0x07
#define RT200_PAL_H					0x08
#define RT200_PAL_I					0x09
#define RT200_PAL_N					0x0a
#define RT200_PAL_Ncomb				0x0b
#define RT200_PAL_M					0x0c
#define RT200_PAL_60				0x0d
#define RT200_SECAM					0x0e
#define RT200_SECAM_B				0x0f
#define RT200_SECAM_D				0x10
#define RT200_SECAM_G				0x11
#define RT200_SECAM_H				0x12
#define RT200_SECAM_K				0x13
#define RT200_SECAM_K1				0x14
#define RT200_SECAM_L				0x15
#define RT200_SECAM_L1				0x16
#define RT200_480i					0x17
#define RT200_480p					0x18
#define RT200_576i					0x19
#define RT200_720p					0x1a
#define RT200_1080i					0x1b

struct rt200_microc_head
{
	unsigned int device_id;
	unsigned int vendor_id;
	unsigned int revision_id;
	unsigned int num_seg;
};

struct rt200_microc_seg
{
	unsigned int num_bytes;
	unsigned int download_dst;
	unsigned int crc_val;

	unsigned char* data;
	struct rt200_microc_seg* next;
};


struct rt200_microc_data
{
	struct rt200_microc_head		microc_head;
	struct rt200_microc_seg*		microc_seg_list;
};

#endif
