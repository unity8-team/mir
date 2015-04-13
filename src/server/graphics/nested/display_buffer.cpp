/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "display_buffer.h"

#include "host_connection.h"
#include "host_stream.h"
#include "mir/input/input_dispatcher.h"
#include "mir/graphics/pixel_format_utils.h"
#include "mir/graphics/buffer.h"
#include "mir/events/event_private.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <algorithm>

namespace mg = mir::graphics;
namespace mgn = mir::graphics::nested;
namespace geom = mir::geometry;

mgn::detail::DisplayBuffer::DisplayBuffer(
    EGLDisplayHandle const& egl_display,
    std::shared_ptr<HostSurface> const& host_surface,
    geometry::Rectangle const& area,
    std::shared_ptr<input::InputDispatcher> const& dispatcher,
    MirPixelFormat preferred_format) :
    uses_alpha_{mg::contains_alpha(preferred_format)},
    egl_display(egl_display),
    host_surface{host_surface},
    egl_config{egl_display.choose_windowed_es_config(preferred_format)},
    egl_context{egl_display, eglCreateContext(egl_display, egl_config, egl_display.egl_context(), nested_egl_context_attribs)},
    area{area.top_left, area.size},
    dispatcher{dispatcher},
    egl_surface{egl_display, host_surface->egl_native_window(), egl_config}
{
    host_surface->set_event_handler(event_thunk, this);
}

geom::Rectangle mgn::detail::DisplayBuffer::view_area() const
{
    return area;
}

void mgn::detail::DisplayBuffer::make_current()
{
    if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) != EGL_TRUE)
        BOOST_THROW_EXCEPTION(std::runtime_error("Nested Mir Display Error: Failed to update EGL surface.\n"));
}

void mgn::detail::DisplayBuffer::release_current()
{
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void mgn::detail::DisplayBuffer::gl_swap_buffers()
{
    //show the primary surface
    eglSwapBuffers(egl_display, egl_surface);
}

bool mgn::detail::DisplayBuffer::post_renderables_if_optimizable(RenderableList const& renderables)
{
    std::unique_lock<decltype(stream_mutex)> lk(stream_mutex);

    //validate that we have a host-allocated stream for every renderable we're given
    for(auto const& renderable : renderables)
    {
        auto buffer = renderable->buffer();
        auto stream_it = std::find_if(streams.begin(), streams.end(),
            [&buffer](HostStream* stream)
            {
                return buffer->id() == stream->current_buffer().lock()->id(); 
            });

        //if we have any stream that needs to be rendered but we don't have a stream associated
        //we can't passthrough the render
        if (stream_it ==  streams.end())
            return false;
    }

    //hide the primary surface

    //TODO: work out how to submit changes synchronously to the client API
    for(auto const& renderable : renderables)
    {
        //TODO: arrange the surfaces via the api according to how RenderableList is structured
        auto buffer = renderable->buffer();
        auto stream_it = std::find_if(streams.begin(), streams.end(),
            [&buffer](HostStream* stream)
            {
                return buffer->id() == stream->current_buffer().lock()->id(); 
            });
        (*stream_it)->swap();
    }

    return true;
}

MirOrientation mgn::detail::DisplayBuffer::orientation() const
{
    /*
     * Always normal orientation. The real rotation is handled by the
     * native display.
     */
    return mir_orientation_normal;
}

bool mgn::detail::DisplayBuffer::uses_alpha() const
{
    return uses_alpha_;
}

mgn::detail::DisplayBuffer::~DisplayBuffer() noexcept
{
}

void mgn::detail::DisplayBuffer::event_thunk(
    MirSurface* /*surface*/,
    MirEvent const* event,
    void* context)
try
{
    static_cast<mgn::detail::DisplayBuffer*>(context)->mir_event(*event);
}
catch (std::exception const&)
{
    // Just in case: do not allow exceptions to propagate.
}

void mgn::detail::DisplayBuffer::mir_event(MirEvent const& event)
{
    if (event.type == mir_event_type_motion)
    {
        auto my_event = event;
        my_event.motion.x_offset += area.top_left.x.as_float();
        my_event.motion.y_offset += area.top_left.y.as_float();
        dispatcher->dispatch(my_event);
    }
    else
    {
        dispatcher->dispatch(event);
    }
}

void mgn::detail::DisplayBuffer::link_with_stream(HostStream* stream)
{
    std::unique_lock<decltype(stream_mutex)> lk(stream_mutex);
    streams.insert(stream);
}

void mgn::detail::DisplayBuffer::unlink_from_stream(HostStream* stream)
{
    std::unique_lock<decltype(stream_mutex)> lk(stream_mutex);
    auto it = streams.find(stream);
    if (it != streams.end())
        streams.erase(it);
}
