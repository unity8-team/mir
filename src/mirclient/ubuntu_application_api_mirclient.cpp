/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

// C APIs
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/ui/window.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/clipboard.h>
#include <ubuntu/application/ui/display.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

#include <mir_toolkit/mir_client_library.h>

extern "C"
{
// TODO: Application description/options

UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    (void) description;
    (void) options;
    return (UApplicationInstance*) NULL;
}

void ua_ui_set_clipboard_content(void* content, size_t content_size)
{
    // TODO: Implement
    (void) content;
    (void) content_size;
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    // TODO: Implement
    *out_content = NULL;
    *out_content_size = 0;
}

UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    // TODO: Implement
    (void) index;
    return (UAUiDisplay*) NULL;
}

void ua_ui_display_destroy(UAUiDisplay* display)
{
    // TODO: Implement
    (void) display;
}

uint32_t ua_ui_display_query_horizontal_res(UAUiDisplay* display)
{
    // TODO: Implement
    (void) display;
    return 0;
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    // TODO: Implement
    (void) display;
    return 0;
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    // TODO: Implement
    return (UAUiWindowProperties*) NULL;
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* properties)
{
    // TODO: Implement
    (void) properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* properties, const char* title, size_t title_length)
{
    // TODO: Implement
    (void) properties;
    (void) title;
    (void) title_length;
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* properties)
{
    // TODO: Implement
    (void) properties;
    return NULL;
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    // TODO: Implement
    (void) properties;
    (void) role;
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* properties, UAUiWindowInputEventCb cb, void* ctx)
{
    // TODO: Implement
    (void) properties;
    (void) cb;
    (void) ctx;
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* instance, UAUiWindowProperties* properties)
{
    // TODO: Implement
    (void) instance;
    (void) properties;
    return (UAUiWindow*) NULL;
}

void ua_ui_window_destroy(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
}

UStatus ua_ui_window_move(UAUiWindow* window, uint32_t x, uint32_t y)
{
    // TODO: Implement
    (void) window;
    (void) x;
    (void) y;
    return (UStatus) 0;
}

UStatus ua_ui_window_resize(UAUiWindow* window, uint32_t width, uint32_t height)
{
    // TODO: Implement
    (void) window;
    (void) width;
    (void) height;
    return (UStatus) 0;
}

UStatus ua_ui_window_hide(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
    return (UStatus) 0;
}

UStatus ua_ui_window_show(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
    return (UStatus) 0;
}

void ua_ui_window_request_fullscreen(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
}

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* window)
{
    // TODO: Implement
    (void) window;
    return (EGLNativeWindowType) 0;
}

// TODO: Sensors
}
