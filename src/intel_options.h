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
	OPTION_TILING_FB,
	OPTION_TILING_2D,
	OPTION_SHADOW,
	OPTION_SWAPBUFFERS_WAIT,
	OPTION_TRIPLE_BUFFER,
#ifdef INTEL_XVMC
	OPTION_XVMC,
#endif
	OPTION_PREFER_OVERLAY,
	OPTION_DEBUG_FLUSH_BATCHES,
	OPTION_DEBUG_FLUSH_CACHES,
	OPTION_DEBUG_WAIT,
	OPTION_HOTPLUG,
	OPTION_RELAXED_FENCING,
	OPTION_USE_SNA,
#ifdef USE_SNA
	OPTION_THROTTLE,
	OPTION_VMAP,
	OPTION_ZAPHOD,
	OPTION_DELAYED_FLUSH,
#endif
#ifdef USE_UXA
	OPTION_FALLBACKDEBUG,
	OPTION_BUFFER_CACHE,
#endif
	NUM_OPTIONS,
};

extern const OptionInfoRec intel_options[];
OptionInfoPtr intel_options_get(ScrnInfoPtr scrn);

#endif /* INTEL_OPTIONS_H */
