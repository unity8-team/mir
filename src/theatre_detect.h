/*************************************************************************************
 * $Id$
 * 
 * Created by Bogdan D. bogdand@users.sourceforge.net 
 * License: GPL
 *
 * $Log$
 * Revision 1.2  2005/07/01 22:43:11  daniels
 * Change all misc.h and os.h references to <X11/foo.h>.
 *
 *
 ************************************************************************************/

#ifndef __THEATRE_DETECT_H__
#define __THEATRE_DETECT_H__

/*
 * Created by Bogdan D. bogdand@users.sourceforge.net
 */


TheatrePtr DetectTheatre(GENERIC_BUS_Ptr b);


#define TheatreDetectSymbolsList  \
		"DetectTheatre"

#ifdef XFree86LOADER

#define xf86_DetectTheatre         ((TheatrePtr (*)(GENERIC_BUS_Ptr))LoaderSymbol("DetectTheatre"))

#else

#define xf86_DetectTheatre             DetectTheatre

#endif		

#endif
