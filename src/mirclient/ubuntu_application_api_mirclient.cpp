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

#include <ubuntu/application/ui/ubuntu_application_ui.h>
    
void
ubuntu_application_ui_init(int argc, char**argv)
{
}

StageHint
ubuntu_application_ui_setup_get_stage_hint()
{
    // TODO: Implement ~racarr
    return MAIN_STAGE_HINT;
}

FormFactorHint
ubuntu_application_ui_setup_get_form_factor_hint()
{
    // TODO: Implement ~racarr
    return DESKTOP_FORM_FACTOR_HINT;
}

void
ubuntu_application_ui_start_a_new_session(SessionCredentials* creds)
{
    // TODO: Implement ~racarr
}

EGLNativeDisplayType
ubuntu_application_ui_get_native_display()
{
    return (EGLNativeDisplayType)0;
}

void
ubuntu_application_ui_set_clipboard_content(void* data, size_t size)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_get_clipboard_content(void** data, size_t* size)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_create_display_info(ubuntu_application_ui_physical_display_info* info, size_t index)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_destroy_display_info(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
}

int32_t
ubuntu_application_ui_query_horizontal_resolution(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (int32_t) 0;
}

int32_t
ubuntu_application_ui_query_vertical_resolution(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (int32_t) 0;
}

float
ubuntu_application_ui_query_horizontal_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (float)0.0;
}

float
ubuntu_application_ui_query_vertical_dpi(ubuntu_application_ui_physical_display_info info)
{
    // TODO: Implement ~racarr
    return (float)0.0;
}

void
ubuntu_application_ui_create_surface(ubuntu_application_ui_surface* out_surface,
                                     const char* title,
                                     int width,
                                     int height,
                                     SurfaceRole role,
                                     uint32_t flags,
                                     input_event_cb cb,
                                     void* ctx)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_request_fullscreen_for_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_destroy_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

EGLNativeWindowType
ubuntu_application_ui_surface_to_native_window_type(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
    return (EGLNativeWindowType)0;
}

void
ubuntu_application_ui_show_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_hide_surface(ubuntu_application_ui_surface surface)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_move_surface_to(ubuntu_application_ui_surface surface,
                                      int x, int y)
{
    // TODO: Implement ~racarr
}

void
ubuntu_application_ui_resize_surface_to(ubuntu_application_ui_surface surface,
                                        int w, int h)
{
    // TODO: Implement ~racarr
}
