/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include <utils/misc.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/Region.h>
#include <ui/Rect.h>

using namespace android;
static sp<SurfaceComposerClient> android_client;
static sp<Surface> android_surface;
static sp<SurfaceControl> android_surface_control;

void kvant_sf_init(EGLNativeWindowType *egl_window, int x, int y, int w, int h, float alpha)
{
	int fmt = PIXEL_FORMAT_RGBA_8888;

	android_client = new SurfaceComposerClient;
	if (android_client == NULL) {
		printf("failed to create client\n");
	}

	android_surface_control = android_client->createSurface(
                                    String8("test"), w, h, fmt, 0x0);

    android_surface = android_surface_control->getSurface();

	if ((android_surface == NULL) || (android_surface_control == NULL)) {
		printf("failed to create surface\n");
	}

	android_client->openGlobalTransaction();
	android_surface_control->setPosition(x, y);
	android_surface_control->setLayer(INT_MAX);
	android_surface_control->setAlpha(alpha);
	android_client->closeGlobalTransaction();

    *egl_window = (EGLNativeWindowType) android_surface.get();
}

void kvant_sf_shutdown()
{
    /* these are static sp's so we have to set them to null to clean them up */
    android_client = NULL;
    android_surface = NULL;
    android_surface_control = NULL;
}
