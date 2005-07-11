/*************************************************************************************
 * $Id$
 * 
 * Created by Bogdan D. bogdand@users.sourceforge.net 
 * License: GPL
 *
 * $Log$
 * Revision 1.3  2005/07/11 02:29:45  ajax
 * Prep for modular builds by adding guarded #include "config.h" everywhere.
 *
 * Revision 1.2  2005/07/01 22:43:11  daniels
 * Change all misc.h and os.h references to <X11/foo.h>.
 *
 *
 ************************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "generic_bus.h"
#include "theatre.h"
#include "theatre_reg.h"

#undef read
#undef write
#undef ioctl

static Bool theatre_read(TheatrePtr t,CARD32 reg, CARD32 *data)
{
   if(t->theatre_num<0)return FALSE;
   return t->VIP->read(t->VIP, ((t->theatre_num & 0x3)<<14) | reg,4, (CARD8 *) data);
}

static Bool theatre_write(TheatrePtr t,CARD32 reg, CARD32 data)
{
   if(t->theatre_num<0)return FALSE;
   return t->VIP->write(t->VIP,((t->theatre_num & 0x03)<<14) | reg,4, (CARD8 *) &data);
}

#define RT_regr(reg,data)	theatre_read(t,(reg),(data))
#define RT_regw(reg,data)	theatre_write(t,(reg),(data))
#define VIP_TYPE      "ATI VIP BUS"


TheatrePtr DetectTheatre(GENERIC_BUS_Ptr b)
{
   TheatrePtr t;  
   CARD32 i;
   CARD32 val;
   char s[20];
   
   b->ioctl(b,GB_IOCTL_GET_TYPE,20,s);
   if(strcmp(VIP_TYPE, s)){
   xf86DrvMsg(b->scrnIndex, X_ERROR, "DetectTheatre must be called with bus of type \"%s\", not \"%s\"\n",
          VIP_TYPE, s);
   return NULL;
   }
   
   t = xcalloc(1,sizeof(TheatreRec));
   t->VIP = b;
   t->theatre_num = -1;
   t->mode=MODE_UNINITIALIZED;

   b->read(b, VIP_VIP_VENDOR_DEVICE_ID, 4, (CARD8 *)&val);
   for(i=0;i<4;i++)
   {
	if(b->read(b, ((i & 0x03)<<14) | VIP_VIP_VENDOR_DEVICE_ID, 4, (CARD8 *)&val))
        {
	  if(val)xf86DrvMsg(b->scrnIndex, X_INFO, "Device %d on VIP bus ids as 0x%08x\n",i,val);
	  if(t->theatre_num>=0)continue; /* already found one instance */
	  switch(val){
	  	case RT100_ATI_ID:
	           t->theatre_num=i;
		   t->theatre_id=RT100_ATI_ID;
		   break;
		case RT200_ATI_ID:
	           t->theatre_num=i;
		   t->theatre_id=RT200_ATI_ID;
		   break;
                }
	} else {
	  xf86DrvMsg(b->scrnIndex, X_INFO, "No response from device %d on VIP bus\n",i);	
	}
   }
   if(t->theatre_num>=0)xf86DrvMsg(b->scrnIndex, X_INFO, "Detected Rage Theatre as device %d on VIP bus with id 0x%08x\n",t->theatre_num,t->theatre_id);

   if(t->theatre_num < 0)
   {
   xfree(t);
   return NULL;
   }

   RT_regr(VIP_VIP_REVISION_ID, &val);
   xf86DrvMsg(b->scrnIndex, X_INFO, "Detected Rage Theatre revision %8.8X\n", val);

#if 0
DumpRageTheatreRegsByName(t);
#endif
	
   return t;
}

