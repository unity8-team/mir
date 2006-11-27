/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
#ifndef _I830_XF86CRTC_H_
#define _I830_XF86CRTC_H_

#include <edid.h>
#include "i830_xf86Modes.h"

typedef struct _I830_xf86Crtc I830_xf86CrtcRec, *I830_xf86CrtcPtr;

typedef struct _I830_xf86CrtcFuncs {
   /**
    * Turns the crtc on/off, or sets intermediate power levels if available.
    *
    * Unsupported intermediate modes drop to the lower power setting.  If the
    * mode is DPMSModeOff, the crtc must be disabled, as the DPLL may be
    * disabled afterwards.
    */
   void
    (*dpms)(I830_xf86CrtcPtr		crtc,
	    int				mode);

   /**
    * Saves the crtc's state for restoration on VT switch.
    */
   void
    (*save)(I830_xf86CrtcPtr		crtc);

   /**
    * Restore's the crtc's state at VT switch.
    */
   void
    (*restore)(I830_xf86CrtcPtr		crtc);

    /**
     * Clean up driver-specific bits of the crtc
     */
    void
    (*destroy) (I830_xf86CrtcPtr	crtc);
} I830_xf86CrtcFuncsRec, *I830_xf86CrtcFuncsPtr;

struct _I830_xf86Crtc {
    /**
     * Associated ScrnInfo
     */
    ScrnInfoPtr		scrn;
    
    /**
     * Active state of this CRTC
     *
     * Set when this CRTC is driving one or more outputs 
     */
    Bool	    enabled;
    
    /**
     * Position on screen
     *
     * Locates this CRTC within the frame buffer
     */
    int		    x, y;
    
    /** Track whether cursor is within CRTC range  */
    Bool	    cursorInRange;
    
    /** Track state of cursor associated with this CRTC */
    Bool	    cursorShown;
    
    /**
     * Active mode
     *
     * This reflects the mode as set in the CRTC currently
     * It will be cleared when the VT is not active or
     * during server startup
     */
    DisplayModeRec  curMode;
    
    /**
     * Desired mode
     *
     * This is set to the requested mode, independent of
     * whether the VT is active. In particular, it receives
     * the startup configured mode and saves the active mode
     * on VT switch.
     */
    DisplayModeRec  desiredMode;
    
    /** crtc-specific functions */
    const I830_xf86CrtcFuncsRec *funcs;

    /**
     * Driver private
     *
     * Holds driver-private information
     */
    void	    *driver_private;

#ifdef RANDR_12_INTERFACE
    /**
     * RandR crtc
     *
     * When RandR 1.2 is available, this
     * points at the associated crtc object
     */
    RRCrtcPtr	    randr_crtc;
#else
    void	    *randr_crtc;
#endif
};

typedef struct _I830_xf86Output I830_xf86OutputRec, *I830_xf86OutputPtr;

typedef struct _I830_xf86OutputFuncs {
    /**
     * Turns the output on/off, or sets intermediate power levels if available.
     *
     * Unsupported intermediate modes drop to the lower power setting.  If the
     * mode is DPMSModeOff, the output must be disabled, as the DPLL may be
     * disabled afterwards.
     */
    void
    (*dpms)(I830_xf86OutputPtr	output,
	    int			mode);

    /**
     * Saves the output's state for restoration on VT switch.
     */
    void
    (*save)(I830_xf86OutputPtr		output);

    /**
     * Restore's the output's state at VT switch.
     */
    void
    (*restore)(I830_xf86OutputPtr	output);

    /**
     * Callback for testing a video mode for a given output.
     *
     * This function should only check for cases where a mode can't be supported
     * on the pipe specifically, and not represent generic CRTC limitations.
     *
     * \return MODE_OK if the mode is valid, or another MODE_* otherwise.
     */
    int
    (*mode_valid)(I830_xf86OutputPtr	output,
		  DisplayModePtr	pMode);

    /**
     * Callback for setting up a video mode before any crtc/dpll changes.
     *
     * \param pMode the mode that will be set, or NULL if the mode to be set is
     * unknown (such as the restore path of VT switching).
     */
    void
    (*pre_set_mode)(I830_xf86OutputPtr  output,
		    DisplayModePtr	pMode);

    /**
     * Callback for setting up a video mode after the DPLL update but before
     * the plane is enabled.
     */
    void
    (*post_set_mode)(I830_xf86OutputPtr	output,
		     DisplayModePtr	pMode);

    /**
     * Probe for a connected output, and return detect_status.
     */
    enum detect_status
    (*detect)(I830_xf86OutputPtr	output);

    /**
     * Query the device for the modes it provides.
     *
     * This function may also update MonInfo, mm_width, and mm_height.
     *
     * \return singly-linked list of modes or NULL if no modes found.
     */
    DisplayModePtr
    (*get_modes)(I830_xf86OutputPtr	output);

    /**
     * Clean up driver-specific bits of the output
     */
    void
    (*destroy) (I830_xf86OutputPtr	output);
} I830_xf86OutputFuncsRec, *I830_xf86OutputFuncsPtr;

struct _I830_xf86Output {
    /**
     * Associated ScrnInfo
     */
    ScrnInfoPtr		scrn;
    /**
     * Currently connected crtc (if any)
     *
     * If this output is not in use, this field will be NULL.
     */
    I830_xf86CrtcPtr	crtc;
    /**
     * List of available modes on this output.
     *
     * This should be the list from get_modes(), plus perhaps additional
     * compatible modes added later.
     */
    DisplayModePtr	probed_modes;

    /** EDID monitor information */
    xf86MonPtr		MonInfo;

    /** Physical size of the currently attached output device. */
    int			mm_width, mm_height;

    /** Output name */
    char		*name;

    /** output-specific functions */
    const I830_xf86OutputFuncsRec *funcs;

    /** driver private information */
    void		*driver_private;
    
#ifdef RANDR_12_INTERFACE
    /**
     * RandR 1.2 output structure.
     *
     * When RandR 1.2 is available, this points at the associated
     * RandR output structure and is created when this output is created
     */
    RROutputPtr		randr_output;
#else
    void		*randr_output;
#endif
};

/*
 * Crtc functions
 */
I830_xf86CrtcPtr
i830xf86CrtcCreate (ScrnInfoPtr			scrn,
		    const I830_xf86CrtcFuncsRec	*funcs);

void
i830xf86CrtcDestroy (I830_xf86CrtcPtr		xf86_crtc);


/**
 * Allocate a crtc for the specified output
 *
 * Find a currently unused CRTC which is suitable for
 * the specified output
 */

I830_xf86CrtcPtr 
i830xf86AllocCrtc (I830_xf86OutputPtr		output);

/**
 * Free a crtc
 *
 * Mark the crtc as unused by any outputs
 */

void
i830xf86FreeCrtc (I830_xf86CrtcPtr crtc);

/*
 * Output functions
 */
I830_xf86OutputPtr
i830xf86OutputCreate (ScrnInfoPtr		    scrn,
		      const I830_xf86OutputFuncsRec *funcs,
		      const char		    *name);

void
i830xf86OutputDestroy (I830_xf86OutputPtr	xf86_output);

#endif /* _I830_XF86CRTC_H_ */
