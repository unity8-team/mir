/*
 * Copyright (C) 2013-2014 Canonical Ltd
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
#include <ubuntu/application/init.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/display.h>
#include <ubuntu/application/ui/window.h>

extern "C"
{
void u_application_init(void *args)
{
    (void) args;
}

void u_application_finish()
{
}

UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* u_description, UApplicationOptions* u_options)
{
    (void) u_description;
    (void) u_options;
    return NULL;
}

void
u_application_instance_ref(UApplicationInstance *u_instance)
{
    (void) u_instance;
}

void
u_application_instance_unref(UApplicationInstance *u_instance)
{
    (void) u_instance;
}

void
u_application_instance_destroy(UApplicationInstance *instance)
{
    (void) instance;
}

void
u_application_instance_run(UApplicationInstance *instance)
{
    // TODO<papi>: What is this supposed to do? Seems to be no-op on hybris.
    (void) instance;
}

MirConnection*
u_application_instance_get_mir_connection(UApplicationInstance *instance)
{
    (void) instance;
    return nullptr;
}

void ua_ui_set_clipboard_content(void* content, size_t content_size)
{
    // TODO<mir,papi>: Implement. Probably need more arguments?
    (void) content;
    (void) content_size;
}

void ua_ui_get_clipboard_content(void** out_content, size_t* out_content_size)
{
    *out_content = NULL;
    *out_content_size = 0;
}

UAUiDisplay* ua_ui_display_new_with_index(size_t index)
{
    (void) index;
    return NULL;
}

void ua_ui_display_destroy(UAUiDisplay* display)
{
    (void) display;
}

uint32_t ua_ui_display_query_horizontal_res(UAUiDisplay* display)
{
    (void) display;

    return 0;
}

uint32_t ua_ui_display_query_vertical_res(UAUiDisplay* display)
{
    (void) display; // TODO<mir>: Multiple displays

    return 0;
}

EGLNativeDisplayType ua_ui_display_get_native_type(UAUiDisplay* display)
{
    (void) display;
    return NULL;
}

UAUiWindowProperties* ua_ui_window_properties_new_for_normal_window()
{
    return NULL;
}

void ua_ui_window_properties_destroy(UAUiWindowProperties* u_properties)
{
    (void) u_properties;
}

void ua_ui_window_properties_set_titlen(UAUiWindowProperties* u_properties, const char* title, size_t title_length)
{
    (void) u_properties;
    (void) title;
    (void) title_length;
}

const char* ua_ui_window_properties_get_title(UAUiWindowProperties* u_properties)
{
    (void) u_properties;
    return NULL;
}

void ua_ui_window_properties_set_role(UAUiWindowProperties* properties, UAUiWindowRole role)
{
    (void) properties;
    (void) role;
}

void ua_ui_window_properties_set_event_cb_and_ctx(UAUiWindowProperties* u_properties, UAUiWindowEventCb cb, void* ctx)
{
    (void) u_properties;
    (void) cb;
    (void) ctx;
}

void ua_ui_window_properties_set_input_cb_and_ctx(UAUiWindowProperties* u_properties, UAUiWindowInputEventCb cb, void* ctx)
{
    (void) u_properties;
    (void) cb;
    (void) ctx;
}

void ua_ui_window_properties_set_dimensions(UAUiWindowProperties *u_properties, uint32_t width, uint32_t height)
{
    (void) u_properties;
    (void) width;
    (void) height;
}

UAUiWindow* ua_ui_window_new_for_application_with_properties(UApplicationInstance* u_instance, UAUiWindowProperties* u_properties)
{
    (void) u_instance;
    (void) u_properties;
    return NULL;
}

void ua_ui_window_destroy(UAUiWindow* u_window)
{
    (void) u_window;
}

void ua_ui_window_get_size(UAUiWindow* u_window, uint32_t *width, uint32_t *height)
{
    (void) u_window;
    (void) width;
    (void) height;
}

UStatus ua_ui_window_move(UAUiWindow* window, uint32_t x, uint32_t y)
{
    (void) window;
    (void) x;
    (void) y;
    return (UStatus) 0;
}

UStatus ua_ui_window_resize(UAUiWindow* window, uint32_t width, uint32_t height)
{
    (void) window;
    (void) width;
    (void) height;
    return (UStatus) 0;
}

UStatus ua_ui_window_hide(UAUiWindow* window)
{
    (void) window;
    return (UStatus) 0;
}

UStatus ua_ui_window_show(UAUiWindow* window)
{
    (void) window;
    return (UStatus) 0;
}

void ua_ui_window_request_fullscreen(UAUiWindow* window)
{
    (void) window;
}

EGLNativeWindowType ua_ui_window_get_native_type(UAUiWindow* u_window)
{
    (void) (u_window);
    return 0;
}

}
