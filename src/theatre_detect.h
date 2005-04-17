/*************************************************************************************
 * $Id$
 * 
 * Created by Bogdan D. bogdand@users.sourceforge.net 
 * License: GPL
 *
 * $Log$
 * Revision 1.1  2005/04/17 23:09:28  bogdand
 * This is the theatre chip detection module
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
