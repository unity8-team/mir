#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef XFree86LOADER
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#endif
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include "os.h"
#include "xf86.h"
#include "xf86Priv.h"
#define XF86_OS_PRIVS
#include "xf86_OSproc.h"

#include "i830.h"
 
#define ACPI_SOCKET  "/var/run/acpid.socket"
#define ACPI_EVENTS  "/proc/acpi/event"

#define ACPI_VIDEO_NOTIFY_SWITCH	0x80
#define ACPI_VIDEO_NOTIFY_PROBE		0x81
#define ACPI_VIDEO_NOTIFY_CYCLE		0x82
#define ACPI_VIDEO_NOTIFY_NEXT_OUTPUT	0x83
#define ACPI_VIDEO_NOTIFY_PREV_OUTPUT	0x84

#define ACPI_VIDEO_NOTIFY_CYCLE_BRIGHTNESS	0x82
#define	ACPI_VIDEO_NOTIFY_INC_BRIGHTNESS	0x83
#define ACPI_VIDEO_NOTIFY_DEC_BRIGHTNESS	0x84
#define ACPI_VIDEO_NOTIFY_ZERO_BRIGHTNESS	0x85
#define ACPI_VIDEO_NOTIFY_DISPLAY_OFF		0x86

#define ACPI_VIDEO_HEAD_INVALID		(~0u - 1)
#define ACPI_VIDEO_HEAD_END		(~0u)

static void I830CloseACPI(void);
pointer I830ACPIihPtr = NULL;
PMClose I830ACPIOpen(void);

#define LINE_LENGTH 80

#define MAX_NO_EVENTS 10

static int
I830ACPIGetEventFromOs(int fd, pmEvent *events, int num)
{
    char ev[LINE_LENGTH];
    int n;

    memset(ev, 0, LINE_LENGTH);

    n = read( fd, ev, LINE_LENGTH );

    /* Check that we have a video event */
    if (strstr(ev, "video") == ev) {
	char *video = NULL;
	char *GFX = NULL;
	char *notify = NULL;
	char *data = NULL; /* doesn't appear to be used in the kernel */
	unsigned long int notify_l, data_l;

	video = strtok(ev, "video");

	GFX = strtok(NULL, " ");
#if 0
	ErrorF("GFX: %s\n",GFX);
#endif

	notify = strtok(NULL, " ");
	notify_l = strtoul(notify, NULL, 16);
#if 0
	ErrorF("notify: 0x%lx\n",notify_l);
#endif

	data = strtok(NULL, " ");
	data_l = strtoul(data, NULL, 16);
#if 0
	ErrorF("data: 0x%lx\n",data_l);
#endif

	/* Currently we don't differentiate events */
	switch (notify_l) {
		case ACPI_VIDEO_NOTIFY_SWITCH:
			break;
		case ACPI_VIDEO_NOTIFY_PROBE:
			break;
		case ACPI_VIDEO_NOTIFY_CYCLE:
			break;
		case ACPI_VIDEO_NOTIFY_NEXT_OUTPUT:
			break;
		case ACPI_VIDEO_NOTIFY_PREV_OUTPUT:
			break;
		default:
			break;
	}

	/* We should probably add the ACPI events to the common layer */
        events[0] = XF86_APM_CAPABILITY_CHANGED;

	return 1;
    }
    
    return 0;
}

static void
I830HandlePMEvents(int fd, pointer data)
{
    pmEvent events[MAX_NO_EVENTS];
    int i,j,n;

    if (!I830ACPIGetEventFromOs)
	return;

    if ((n = I830ACPIGetEventFromOs(fd,events,MAX_NO_EVENTS))) {
	do {
	    for (j = 0; j < n; j++) {
		xf86EnterServerState(SETUP);
		for (i = 0; i < xf86NumScreens; i++) {
	    	    xf86EnableAccess(xf86Screens[i]);
	    	    if (xf86Screens[i]->PMEvent)
			xf86Screens[i]->PMEvent(i,events[j],FALSE);
		}
		xf86EnterServerState(OPERATING);
	    }
	    break;
	} while (1);
    }
}

PMClose
I830ACPIOpen(void)
{
    int fd;    
    struct sockaddr_un addr;
    int r = -1;

#ifdef DEBUG
    ErrorF("ACPI: OSPMOpen called\n");
#endif
    if (I830ACPIihPtr)
	return NULL;
   
#ifdef DEBUG
    ErrorF("ACPI: Opening device\n");
#endif
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) > -1) {
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, ACPI_SOCKET);
	if ((r = connect(fd, (struct sockaddr*)&addr, sizeof(addr))) == -1) {
	    shutdown(fd, 2);
            close(fd);
	    fd = -1;
	}
    }

    /* acpid's socket isn't available, so try going direct */
    if (fd == -1) {
        if ((fd = open(ACPI_EVENTS, O_RDONLY)) < 0) {
	    xf86MsgVerb(X_WARNING,3,"Open ACPI failed (%s) (%s)\n", ACPI_EVENTS,
	    	strerror(errno));
	    return NULL;
    	}
    }

    I830ACPIihPtr = xf86AddInputHandler(fd,I830HandlePMEvents,NULL);
    xf86MsgVerb(X_INFO,3,"Open ACPI successful (%s)\n", (r != -1) ? ACPI_SOCKET : ACPI_EVENTS);

    return I830CloseACPI;
}

static void
I830CloseACPI(void)
{
    int fd;
    
#ifdef DEBUG
   ErrorF("ACPI: Closing device\n");
#endif
    if (I830ACPIihPtr) {
	fd = xf86RemoveInputHandler(I830ACPIihPtr);
	shutdown(fd, 2);
        close(fd);
	I830ACPIihPtr = NULL;
    }
}
