/* demo code for how to render on android without any java or the ndk
   author: kevin.dubois@canonical.com */
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <utils/misc.h>

#include <gui/SurfaceComposerClient.h>
#include <ui/Region.h>
#include <ui/Rect.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "eglapp.h"

using namespace android;
static sp<SurfaceComposerClient> android_client;
static sp<Surface> android_surface;
static sp<SurfaceControl> android_surface_control;

sp<Surface> sf_client_init(int x, int y, int w, int h, float alpha)
{
	int fmt = PIXEL_FORMAT_RGBA_8888;

	if (android_client == NULL) {
		printf("failed to create client\n");
		return 0;
	}

	android_surface_control = android_client->createSurface(
                                    String8("test"), w, h, fmt, 0x0);

    android_surface = android_surface_control->getSurface();

	if (android_surface == NULL) {
		printf("failed to create surface controller\n");
		return 0;
	}
	if (android_surface_control == NULL) {
		printf("failed to get surface\n");
		return 0;
	}

	android_client->openGlobalTransaction();
	android_surface_control->setPosition(x, y);
	android_surface_control->setLayer(INT_MAX);
	android_surface_control->setAlpha(alpha);
	android_client->closeGlobalTransaction();

	return android_surface;
}

int main(int argc, char **argv)
{
	sp<Surface> surf = sf_client_init(0, 0, 512, 512, 1.0f);
    sp<ANativeWindow> anw = surf;
    EGLNativeWindowType egl_window = anw.get();

    if (!kvant_egl_init(EGL_DEFAULT_DISPLAY, egl_window))
    {
        printf("Can't initialize EGL\n");
        return 1;
    }

#if 0
	demo_render_loop(200, 200, 500, 500);

    /* these are static sp's so we have to set them to null to clean them up */
    android_client = NULL;
    android_surface = NULL;
    android_surface_control = NULL;
#endif
	return 0;
}
