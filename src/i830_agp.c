/*
 * Abstraction of the AGP GART interface.
 *
 * This version is for both Linux and FreeBSD.
 *
 * Copyright � 2000 VA Linux Systems, Inc.
 * Copyright � 2001 The XFree86 Project, Inc.
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/lnx_agp.c,v 3.11 2003/04/03 22:47:42 dawes Exp $ */

#if defined(linux)
#include <sys/types.h>
#include <linux/agpgart.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/agpio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "i830.h"


#ifndef AGP_DEVICE
#define AGP_DEVICE		"/dev/agpgart"
#endif
/* AGP page size is independent of the host page size. */
#ifndef AGP_PAGE_SIZE
#define AGP_PAGE_SIZE		4096
#endif
#define AGPGART_MAJOR_VERSION	0
#define AGPGART_MINOR_VERSION	99

static int gartFd = -1;
static int acquiredScreen = -1;
static Bool initDone = FALSE;
/*
 * Close /dev/agpgart.  This frees all associated memory allocated during
 * this server generation.
 */
Bool
I830GARTCloseScreen(int screenNum)
{
	if(gartFd != -1) {
		close(gartFd);
		acquiredScreen = -1;
		gartFd = -1;
		initDone = FALSE;
	}
	return TRUE;
}

/*
 * Open /dev/agpgart.  Keep it open until I830GARTCloseScreen is called.
 */
static Bool
GARTInit(int screenNum)
{
	struct _agp_info agpinf;

	if (initDone)
		return (gartFd != -1);

	initDone = TRUE;

	if (gartFd == -1)
		gartFd = open(AGP_DEVICE, O_RDWR, 0);
	else
		return FALSE;

	if (gartFd == -1) {
	    xf86DrvMsg(screenNum, X_ERROR,
		"I830GARTInit: Unable to open " AGP_DEVICE " (%s)\n",
		strerror(errno));
	    return FALSE;
	}

	I830AcquireGART(-1);
	/* Check the kernel driver version. */
	if (ioctl(gartFd, AGPIOC_INFO, &agpinf) != 0) {
		xf86DrvMsg(screenNum, X_ERROR,
			"I830GARTInit: AGPIOC_INFO failed (%s)\n", strerror(errno));
		close(gartFd);
		gartFd = -1;
		return FALSE;
	}
	I830ReleaseGART(-1);

#if defined(linux)
	/* Per Dave Jones, every effort will be made to keep the 
	 * agpgart interface backwards compatible, so allow all 
	 * future versions.
	 */
	if (
#if (AGPGART_MAJOR_VERSION > 0) /* quiet compiler */
	    agpinf.version.major < AGPGART_MAJOR_VERSION ||
#endif
	    (agpinf.version.major == AGPGART_MAJOR_VERSION &&
	     agpinf.version.minor < AGPGART_MINOR_VERSION)) {
		xf86DrvMsg(screenNum, X_ERROR,
			"GARTInit: Kernel agpgart driver version is not current"
			" (%d.%d vs %d.%d)\n",
			agpinf.version.major, agpinf.version.minor,
			AGPGART_MAJOR_VERSION, AGPGART_MINOR_VERSION);
		close(gartFd);
		gartFd = -1;
		return FALSE;
	}
#endif
	
	return TRUE;
}

Bool
I830AgpGARTSupported()
{
	return GARTInit(-1);
}

AgpInfoPtr
I830GetAGPInfo(int screenNum)
{
	struct _agp_info agpinf;
	AgpInfoPtr info;

	if (!GARTInit(screenNum))
		return NULL;


	if ((info = xcalloc(sizeof(AgpInfo), 1)) == NULL) {
		xf86DrvMsg(screenNum, X_ERROR,
			   "I830GetAGPInfo: Failed to allocate AgpInfo\n");
		return NULL;
	}

	if (ioctl(gartFd, AGPIOC_INFO, &agpinf) != 0) {
		xf86DrvMsg(screenNum, X_ERROR,
			   "I830GetAGPInfo: AGPIOC_INFO failed (%s)\n",
			   strerror(errno));
		return NULL;
	}

	info->bridgeId = agpinf.bridge_id;
	info->agpMode = agpinf.agp_mode;
	info->base = agpinf.aper_base;
	info->size = agpinf.aper_size;
	info->totalPages = agpinf.pg_total;
	info->systemPages = agpinf.pg_system;
	info->usedPages = agpinf.pg_used;

	return info;
}

/*
 * XXX If multiple screens can acquire the GART, should we have a reference
 * count instead of using acquiredScreen?
 */

Bool
I830AcquireGART(int screenNum)
{
	if (screenNum != -1 && !GARTInit(screenNum))
		return FALSE;

	if (screenNum == -1 || acquiredScreen != screenNum) {
		if (ioctl(gartFd, AGPIOC_ACQUIRE, 0) != 0) {
			xf86DrvMsg(screenNum, X_WARNING,
				"I830AcquireGART: AGPIOC_ACQUIRE failed (%s)\n",
				strerror(errno));
			return FALSE;
		}
		acquiredScreen = screenNum;
	}
	return TRUE;
}

Bool
I830ReleaseGART(int screenNum)
{
	if (screenNum != -1 && !GARTInit(screenNum))
		return FALSE;

	if (acquiredScreen == screenNum) {
		/*
		 * The FreeBSD agp driver removes allocations on release.
		 * The Linux driver doesn't.  I830ReleaseGART() is expected
		 * to give up access to the GART, but not to remove any
		 * allocations.
		 */
#if !defined(linux)
	    if (screenNum == -1)
#endif
	    {
		if (ioctl(gartFd, AGPIOC_RELEASE, 0) != 0) {
			xf86DrvMsg(screenNum, X_WARNING,
				"I830ReleaseGART: AGPIOC_RELEASE failed (%s)\n",
				strerror(errno));
			return FALSE;
		}
		acquiredScreen = -1;
	    }
	    return TRUE;
	}
	return FALSE;
}

int
I830AllocateGARTMemory(int screenNum, unsigned long size, int type,
			unsigned long *physical)
{
	struct _agp_allocate alloc;
	int pages;

	/*
	 * Allocates "size" bytes of GART memory (rounds up to the next
	 * page multiple) or type "type".  A handle (key) for the allocated
	 * memory is returned.  On error, the return value is -1.
	 */

	if (!GARTInit(screenNum) || acquiredScreen != screenNum)
		return -1;

	pages = (size / AGP_PAGE_SIZE);
	if (size % AGP_PAGE_SIZE != 0)
		pages++;

	/* XXX check for pages == 0? */

	alloc.pg_count = pages;
	alloc.type = type;

	if (ioctl(gartFd, AGPIOC_ALLOCATE, &alloc) != 0) {
		if (type != 3)
		   xf86DrvMsg(screenNum, X_WARNING, "I830AllocateGARTMemory: "
			   "allocation of %d pages failed\n\t(%s)\n", pages,
			   strerror(errno));
		return -1;
	}

	if (physical)
		*physical = alloc.physical;

	return alloc.key;
}

Bool
I830DeallocateGARTMemory(int screenNum, int key)
{
	if (!GARTInit(screenNum) || acquiredScreen != screenNum)
		return FALSE;

	if (acquiredScreen != screenNum) {
		xf86DrvMsg(screenNum, X_ERROR,
                   "xf86UnbindGARTMemory: AGP not acquired by this screen\n");
		return FALSE;
	}

	if (ioctl(gartFd, AGPIOC_DEALLOCATE, (int *)key) != 0) {
		xf86DrvMsg(screenNum, X_WARNING,"I830DeAllocateGARTMemory: "
                   "deallocation gart memory with key %d failed\n\t(%s)\n",
                   key, strerror(errno));
		return FALSE;
	}

	return TRUE;
}

/* Bind GART memory with "key" at "offset" */
Bool
I830BindGARTMemory(int screenNum, int key, unsigned long offset)
{
	struct _agp_bind bind;
	int pageOffset;

	if (!GARTInit(screenNum) || acquiredScreen != screenNum)
		return FALSE;

	if (acquiredScreen != screenNum) {
		xf86DrvMsg(screenNum, X_ERROR,
		      "I830BindGARTMemory: AGP not acquired by this screen\n");
		return FALSE;
	}

	if (offset % AGP_PAGE_SIZE != 0) {
		xf86DrvMsg(screenNum, X_WARNING, "I830BindGARTMemory: "
			   "offset (0x%lx) is not page-aligned (%d)\n",
			   offset, AGP_PAGE_SIZE);
		return FALSE;
	}
	pageOffset = offset / AGP_PAGE_SIZE;

	xf86DrvMsgVerb(screenNum, X_INFO, 3,
		       "I830BindGARTMemory: bind key %d at 0x%08lx "
		       "(pgoffset %d)\n", key, offset, pageOffset);

	bind.pg_start = pageOffset;
	bind.key = key;

	if (ioctl(gartFd, AGPIOC_BIND, &bind) != 0) {
		xf86DrvMsg(screenNum, X_WARNING, "I830BindGARTMemory: "
			   "binding of gart memory with key %d\n"
			   "\tat offset 0x%lx failed (%s)\n",
			   key, offset, strerror(errno));
		return FALSE;
	}

	return TRUE;
}


/* Unbind GART memory with "key" */
Bool
I830UnbindGARTMemory(int screenNum, int key)
{
	struct _agp_unbind unbind;

	if (!GARTInit(screenNum) || acquiredScreen != screenNum)
		return FALSE;

	if (acquiredScreen != screenNum) {
		xf86DrvMsg(screenNum, X_ERROR,
		    "I830UnbindGARTMemory: AGP not acquired by this screen\n");
		return FALSE;
	}

	unbind.priority = 0;
	unbind.key = key;

	if (ioctl(gartFd, AGPIOC_UNBIND, &unbind) != 0) {
		xf86DrvMsg(screenNum, X_WARNING, "I830UnbindGARTMemory: "
			   "unbinding of gart memory with key %d "
			   "failed (%s)\n", key, strerror(errno));
		return FALSE;
	}

	xf86DrvMsgVerb(screenNum, X_INFO, 3,
		       "I830UnbindGARTMemory: unbind key %d\n", key);

	return TRUE;
}


/* XXX Interface may change. */
Bool
I830EnableAGP(int screenNum, CARD32 mode)
{
	agp_setup setup;

	if (!GARTInit(screenNum) || acquiredScreen != screenNum)
		return FALSE;

	setup.agp_mode = mode;
	if (ioctl(gartFd, AGPIOC_SETUP, &setup) != 0) {
		xf86DrvMsg(screenNum, X_WARNING, "I830EnableAGP: "
			   "AGPIOC_SETUP with mode %ld failed (%s)\n",
			   (unsigned long)mode, strerror(errno));
		return FALSE;
	}

	return TRUE;
}

