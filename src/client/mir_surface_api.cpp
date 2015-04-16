/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#define MIR_LOG_COMPONENT "MirSurfaceAPI"

#include "mir_toolkit/mir_surface.h"
#include "mir_toolkit/mir_wait.h"
#include "mir/require.h"

#include "mir_connection.h"
#include "mir_surface.h"
#include "error_connections.h"
#include "mir/uncaught.h"

#include <boost/exception/diagnostic_information.hpp>
#include <functional>

namespace mcl = mir::client;

namespace
{

// assign_result is compatible with all 2-parameter callbacks
void assign_result(void* result, void** context)
{
    if (context)
        *context = result;
}

}

MirSurfaceSpec* mir_connection_create_spec_for_normal_surface(MirConnection* connection,
                                                              int width, int height,
                                                              MirPixelFormat format)
{
    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_normal;
    return spec;
}

MirSurfaceSpec* mir_connection_create_spec_for_menu(MirConnection* connection,
                                                    int width,
                                                    int height,
                                                    MirPixelFormat format,
                                                    MirSurface* parent,
                                                    MirRectangle* rect,
                                                    MirEdgeAttachment edge)
{
    mir::require(mir_surface_is_valid(parent));
    mir::require(rect != nullptr);

    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_menu;
    spec->parent = parent;
    spec->aux_rect = *rect;
    spec->edge_attachment = edge;
    return spec;
}

MirSurfaceSpec* mir_connection_create_spec_for_tooltip(MirConnection* connection,
                                                       int width,
                                                       int height,
                                                       MirPixelFormat format,
                                                       MirSurface* parent,
                                                       MirRectangle* rect)
{
    mir::require(mir_surface_is_valid(parent));
    mir::require(rect != nullptr);

    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_tip;
    spec->parent = parent;
    spec->aux_rect = *rect;
    return spec;
}

MirSurfaceSpec* mir_connection_create_spec_for_dialog(MirConnection* connection,
                                                      int width,
                                                      int height,
                                                      MirPixelFormat format)
{
    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_dialog;
    return spec;
}

MirSurfaceSpec* mir_connection_create_spec_for_input_method(MirConnection* connection,
                                                      int width,
                                                      int height,
                                                      MirPixelFormat format)
{
    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_inputmethod;
    return spec;
}

MirSurfaceSpec* mir_connection_create_spec_for_modal_dialog(MirConnection* connection,
                                                           int width,
                                                           int height,
                                                           MirPixelFormat format,
                                                           MirSurface* parent)
{
    mir::require(mir_surface_is_valid(parent));

    auto spec = new MirSurfaceSpec{connection, width, height, format};
    spec->type = mir_surface_type_dialog;
    spec->parent = parent;

    return spec;
}

MirSurface* mir_surface_create_sync(MirSurfaceSpec* requested_specification)
{
    MirSurface* surface = nullptr;

    mir_wait_for(mir_surface_create(requested_specification,
        reinterpret_cast<mir_surface_callback>(assign_result),
        &surface));

    return surface;
}

MirWaitHandle* mir_surface_create(MirSurfaceSpec* requested_specification,
                                  mir_surface_callback callback, void* context)
{
    mir::require(requested_specification != nullptr);
    mir::require(requested_specification->type.is_set());

    auto conn = requested_specification->connection;
    mir::require(mir_connection_is_valid(conn));

    try
    {
        return conn->create_surface(*requested_specification, callback, context);
    }
    catch (std::exception const& error)
    {
        auto error_surf = new MirSurface{std::string{"Failed to create surface: "} +
                                         boost::diagnostic_information(error)};
        (*callback)(error_surf, context);
        return nullptr;
    }
}

bool mir_surface_spec_set_name(MirSurfaceSpec* spec, char const* name)
{
    spec->surface_name = name;
    return true;
}

bool mir_surface_spec_set_width(MirSurfaceSpec* spec, unsigned width)
{
    spec->width = width;
    return true;
}

bool mir_surface_spec_set_height(MirSurfaceSpec* spec, unsigned height)
{
    spec->height = height;
    return true;
}

bool mir_surface_spec_set_min_width(MirSurfaceSpec* spec, unsigned min_width)
{
    spec->min_width = min_width;
    return true;
}

bool mir_surface_spec_set_min_height(MirSurfaceSpec* spec, unsigned min_height)
{
    spec->min_height = min_height;
    return true;
}

bool mir_surface_spec_set_max_width(MirSurfaceSpec* spec, unsigned max_width)
{
    spec->max_width = max_width;
    return true;
}

bool mir_surface_spec_set_max_height(MirSurfaceSpec* spec, unsigned max_height)
{
    spec->max_height = max_height;
    return true;
}

bool mir_surface_spec_set_pixel_format(MirSurfaceSpec* spec, MirPixelFormat format)
{
    spec->pixel_format = format;
    return true;
}

bool mir_surface_spec_set_buffer_usage(MirSurfaceSpec* spec, MirBufferUsage usage)
{
    spec->buffer_usage = usage;
    return true;
}

bool mir_surface_spec_set_fullscreen_on_output(MirSurfaceSpec* spec, uint32_t output_id)
{
    spec->output_id = output_id;
    spec->state = mir_surface_state_fullscreen;
    return true;
}

bool mir_surface_spec_set_preferred_orientation(MirSurfaceSpec* spec, MirOrientationMode mode)
{
    spec->pref_orientation = mode;
    return true;
}

bool mir_surface_spec_set_buffering_mode(MirSurfaceSpec* spec, MirBufferingMode mode)
{
    spec->buffering_mode = mode;
    return true;
}

void mir_surface_spec_release(MirSurfaceSpec* spec)
{
    delete spec;
}

MirWaitHandle* mir_connection_create_surface(
    MirConnection* connection,
    MirSurfaceParameters const* params,
    mir_surface_callback callback,
    void* context)
{
    MirSurfaceSpec spec{connection, *params};
    return mir_surface_create(&spec, callback, context);
}

MirSurface* mir_connection_create_surface_sync(
    MirConnection* connection,
    MirSurfaceParameters const* params)
{
    MirSurface* surface = nullptr;

    mir_wait_for(mir_connection_create_surface(connection, params,
        reinterpret_cast<mir_surface_callback>(assign_result),
        &surface));

    return surface;
}

__asm__(".symver new_mir_surface_set_event_handler,mir_surface_set_event_handler@@MIR_CLIENT_8.4");
extern "C"
void new_mir_surface_set_event_handler(MirSurface* surface,
                                       mir_surface_event_callback callback,
                                       void* context)
{
    surface->set_event_handler(callback, context);
}

// Deprecated but ABI backward compatible --->
typedef struct MirEventDelegate
{
    mir_surface_event_callback callback;
    void *context;
} MirEventDelegate;

__asm__(".symver old_mir_surface_set_event_handler,mir_surface_set_event_handler@MIR_CLIENT_8");
extern "C"
void old_mir_surface_set_event_handler(MirSurface* surface,
                                       MirEventDelegate const* delegate)
{
    if (delegate)
        surface->set_event_handler(delegate->callback, delegate->context);
    else
        surface->set_event_handler(nullptr, nullptr);
}
// <--- Deprecated

MirEGLNativeWindowType mir_surface_get_egl_native_window(MirSurface* surface)
{
    return mir_buffer_stream_get_egl_native_window(
        mir_surface_get_buffer_stream(surface));
}

bool mir_surface_is_valid(MirSurface* surface)
{
    return MirSurface::is_valid(surface);
}

char const* mir_surface_get_error_message(MirSurface* surface)
{
    return surface->get_error_message();
}

void mir_surface_get_parameters(MirSurface* surface, MirSurfaceParameters* parameters)
{
    *parameters = surface->get_parameters();
}

MirPlatformType mir_surface_get_platform_type(MirSurface* surface)
{
    return mir_buffer_stream_get_platform_type(mir_surface_get_buffer_stream(surface));
}

void mir_surface_get_current_buffer(MirSurface* surface, MirNativeBuffer** buffer_package_out)
{
    mir_buffer_stream_get_current_buffer(mir_surface_get_buffer_stream(surface), buffer_package_out);
}

void mir_surface_get_graphics_region(MirSurface* surface, MirGraphicsRegion* graphics_region)
{
    mir_buffer_stream_get_graphics_region(mir_surface_get_buffer_stream(surface), graphics_region);
}

namespace
{
void buffer_to_surface_thunk(MirBufferStream* /* stream */, void* context)
{
    auto cb = static_cast<std::function<void()>*>(context);
    (*cb)();
}
}

MirWaitHandle* mir_surface_swap_buffers(
    MirSurface* surface,
    mir_surface_callback callback,
    void* context)
try
{
    auto shim_callback = new std::function<void()>;
    *shim_callback = [surface, callback, context, shim_callback] ()
    {
        if (callback)
            callback(surface, context);
        delete shim_callback;
    };
    return mir_buffer_stream_swap_buffers(mir_surface_get_buffer_stream(surface), buffer_to_surface_thunk, shim_callback);
}
catch (std::exception const& ex)
{
    MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    return nullptr;
}

void mir_surface_swap_buffers_sync(MirSurface* surface)
{
    mir_buffer_stream_swap_buffers_sync(
        mir_surface_get_buffer_stream(surface));
}

MirWaitHandle* mir_surface_release(
    MirSurface* surface,
    mir_surface_callback callback, void* context)
{
    try
    {
        return surface->release_surface(callback, context);
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
        return nullptr;
    }
}

void mir_surface_release_sync(MirSurface* surface)
{
    mir_wait_for(mir_surface_release(surface,
        reinterpret_cast<mir_surface_callback>(assign_result),
        nullptr));
}

int mir_surface_get_id(MirSurface* /*surface*/)
{
    return 0;
}

MirWaitHandle* mir_surface_set_type(MirSurface* surf,
                                    MirSurfaceType type)
{
    try
    {
        return surf ? surf->configure(mir_surface_attrib_type, type) : nullptr;
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
        return nullptr;
    }
}

MirSurfaceType mir_surface_get_type(MirSurface* surf)
{
    MirSurfaceType type = mir_surface_type_normal;

    if (surf)
    {
        // Only the client will ever change the type of a surface so it is
        // safe to get the type from a local cache surf->attrib().

        int t = surf->attrib(mir_surface_attrib_type);
        type = static_cast<MirSurfaceType>(t);
    }

    return type;
}

MirWaitHandle* mir_surface_set_state(MirSurface* surf, MirSurfaceState state)
{
    try
    {
        return surf ? surf->configure(mir_surface_attrib_state, state) : nullptr;
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
        return nullptr;
    }
}

MirSurfaceState mir_surface_get_state(MirSurface* surf)
{
    MirSurfaceState state = mir_surface_state_unknown;

    try
    {
        if (surf)
        {
            int s = surf->attrib(mir_surface_attrib_state);

            if (s == mir_surface_state_unknown)
            {
                surf->configure(mir_surface_attrib_state,
                                mir_surface_state_unknown)->wait_for_all();
                s = surf->attrib(mir_surface_attrib_state);
            }

            state = static_cast<MirSurfaceState>(s);
        }
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return state;
}

MirOrientation mir_surface_get_orientation(MirSurface *surface)
{
    return surface->get_orientation();
}

MirWaitHandle* mir_surface_set_swapinterval(MirSurface* surf, int interval)
{
    if ((interval < 0) || (interval > 1))
        return nullptr;

    try
    {
        return surf ? surf->configure(mir_surface_attrib_swapinterval, interval) : nullptr;
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
        return nullptr;
    }
}

int mir_surface_get_swapinterval(MirSurface* surf)
{
    int swap_interval = -1;

    try
    {
        swap_interval = surf ? surf->attrib(mir_surface_attrib_swapinterval) : -1;
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return swap_interval;
}

int mir_surface_get_dpi(MirSurface* surf)
{
    int dpi = -1;

    try
    {
        if (surf)
        {
            dpi = surf->attrib(mir_surface_attrib_dpi);
        }
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return dpi;
}

MirSurfaceFocusState mir_surface_get_focus(MirSurface* surf)
{
    MirSurfaceFocusState state = mir_surface_unfocused;

    try
    {
        if (surf)
        {
            state = static_cast<MirSurfaceFocusState>(surf->attrib(mir_surface_attrib_focus));
        }
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return state;
}

MirSurfaceVisibility mir_surface_get_visibility(MirSurface* surf)
{
    MirSurfaceVisibility state = static_cast<MirSurfaceVisibility>(mir_surface_visibility_occluded);

    try
    {
        if (surf)
        {
            state = static_cast<MirSurfaceVisibility>(surf->attrib(mir_surface_attrib_visibility));
        }
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return state;
}

MirWaitHandle* mir_surface_configure_cursor(MirSurface* surface, MirCursorConfiguration const* cursor)
{
    MirWaitHandle *result = nullptr;
    
    try
    {
        if (surface)
            result = surface->configure_cursor(cursor);
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return result;
}

MirOrientationMode mir_surface_get_preferred_orientation(MirSurface *surf)
{
    mir::require(mir_surface_is_valid(surf));

    MirOrientationMode mode = mir_orientation_mode_any;

    try
    {
        mode = static_cast<MirOrientationMode>(surf->attrib(mir_surface_attrib_preferred_orientation));
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return mode;
}

MirWaitHandle* mir_surface_set_preferred_orientation(MirSurface *surf, MirOrientationMode mode)
{
    mir::require(mir_surface_is_valid(surf));

    MirWaitHandle *result{nullptr};
    try
    {
        result = surf->set_preferred_orientation(mode);
    }
    catch (std::exception const& ex)
    {
        MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    }

    return result;
}

MirBufferStream *mir_surface_get_buffer_stream(MirSurface *surface)
try
{
    return reinterpret_cast<MirBufferStream*>(surface->get_buffer_stream());
}
catch (std::exception const& ex)
{
    MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    return nullptr;
}

namespace
{
MirSurfaceSpec* create_spec_for_changes()
try
{
    return new MirSurfaceSpec{};
}
catch (std::exception const& ex)
{
    MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    std::abort();  // If we just failed to allocate a MirSurfaceSpec returning isn't safe
}
}

MirSurfaceSpec* mir_connection_create_spec_for_changes(MirConnection* connection)
{
    mir::require(mir_connection_is_valid(connection));

    return create_spec_for_changes();
}

void mir_surface_apply_spec(MirSurface* surface, MirSurfaceSpec* spec)
try
{
    mir::require(mir_surface_is_valid(surface));
    mir::require(spec);

    surface->modify(*spec);
}
catch (std::exception const& ex)
{
    MIR_LOG_UNCAUGHT_EXCEPTION(ex);
    // Keep calm and carry on
}

void mir_surface_set_title(MirSurface* surface, char const* name)
{
    auto const spec = create_spec_for_changes();
    mir_surface_spec_set_name(spec, name);
    mir_surface_apply_spec(surface, spec);
    mir_surface_spec_release(spec);
}
