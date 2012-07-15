#ifndef INTEL_OPTIONS_H
#define INTEL_OPTIONS_H

#include <xf86.h>
#include <xf86Opt.h>

/*
 * Note: "ColorKey" is provided for compatibility with the i810 driver.
 * However, the correct option name is "VideoKey".  "ColorKey" usually
 * refers to the tranparency key for 8+24 overlays, not for video overlays.
 */

enum intel_options {
	OPTION_ACCEL_METHOD,
	OPTION_DRI,
	OPTION_VIDEO_KEY,
	OPTION_COLOR_KEY,
	OPTION_TILING_2D,
	OPTION_TILING_FB,
	OPTION_SWAPBUFFERS_WAIT,
	OPTION_PREFER_OVERLAY,
	OPTION_HOTPLUG,
	OPTION_RELAXED_FENCING,
#ifdef INTEL_XVMC
	OPTION_XVMC,
#endif
#ifdef USE_SNA
	OPTION_THROTTLE,
	OPTION_ZAPHOD,
	OPTION_DELAYED_FLUSH,
	OPTION_TEAR_FREE,
	OPTION_CRTC_PIXMAPS,
#endif
#ifdef USE_UXA
	OPTION_FALLBACKDEBUG,
	OPTION_DEBUG_FLUSH_BATCHES,
	OPTION_DEBUG_FLUSH_CACHES,
	OPTION_DEBUG_WAIT,
	OPTION_BUFFER_CACHE,
	OPTION_TRIPLE_BUFFER,
#endif
	NUM_OPTIONS,
};

extern const OptionInfoRec intel_options[];
OptionInfoPtr intel_options_get(ScrnInfoPtr scrn);

#endif /* INTEL_OPTIONS_H */
