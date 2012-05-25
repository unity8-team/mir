#include "intel_options.h"

const OptionInfoRec intel_options[] = {
	{OPTION_ACCEL_METHOD,	"AccelMethod",	OPTV_STRING,	{0},	0},
	{OPTION_DRI,		"DRI",		OPTV_BOOLEAN,	{0},	1},
	{OPTION_COLOR_KEY,	"ColorKey",	OPTV_INTEGER,	{0},	0},
	{OPTION_VIDEO_KEY,	"VideoKey",	OPTV_INTEGER,	{0},	0},
	{OPTION_TILING_2D,	"Tiling",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_TILING_FB,	"LinearFramebuffer",	OPTV_BOOLEAN,	{0},	0},
	{OPTION_SHADOW,	"Shadow",	OPTV_BOOLEAN,	{0},	0},
	{OPTION_SWAPBUFFERS_WAIT, "SwapbuffersWait", OPTV_BOOLEAN,	{0},	1},
	{OPTION_TRIPLE_BUFFER, "TripleBuffer", OPTV_BOOLEAN,	{0},	1},
#ifdef INTEL_XVMC
	{OPTION_XVMC,	"XvMC",		OPTV_BOOLEAN,	{0},	1},
#endif
	{OPTION_PREFER_OVERLAY, "XvPreferOverlay", OPTV_BOOLEAN, {0}, 0},
	{OPTION_DEBUG_FLUSH_BATCHES, "DebugFlushBatches", OPTV_BOOLEAN, {0}, 0},
	{OPTION_DEBUG_FLUSH_CACHES, "DebugFlushCaches", OPTV_BOOLEAN, {0}, 0},
	{OPTION_DEBUG_WAIT, "DebugWait", OPTV_BOOLEAN, {0}, 0},
	{OPTION_HOTPLUG,	"HotPlug",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_RELAXED_FENCING,	"RelaxedFencing",	OPTV_BOOLEAN,	{0},	1},
#ifdef USE_SNA
	{OPTION_THROTTLE,	"Throttle",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_VMAP,	"UseVmap",	OPTV_BOOLEAN,	{0},	1},
	{OPTION_ZAPHOD,	"ZaphodHeads",	OPTV_STRING,	{0},	0},
	{OPTION_DELAYED_FLUSH,	"DelayedFlush",	OPTV_BOOLEAN,	{0},	1},
#endif
#ifdef USE_UXA
	{OPTION_FALLBACKDEBUG, "FallbackDebug", OPTV_BOOLEAN, {0},	0},
	{OPTION_BUFFER_CACHE,       "BufferCache",  OPTV_BOOLEAN,   {0},    1},
#endif
	{-1,			NULL,		OPTV_NONE,	{0},	0}
};

OptionInfoPtr intel_options_get(ScrnInfoPtr scrn)
{
	OptionInfoPtr options;

	xf86CollectOptions(scrn, NULL);
	if (!(options = malloc(sizeof(intel_options))))
		return NULL;

	memcpy(options, intel_options, sizeof(intel_options));
	xf86ProcessOptions(scrn->scrnIndex, scrn->options, options);

	return options;
}
