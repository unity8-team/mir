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

#include "mir/shell/surface.h"
#include "mir/shell/surface_builder.h"
#include "mir/shell/input_targeter.h"
#include "mir/input/input_channel.h"
#include "mir/events/event_sink.h"

#include "mir_toolkit/event.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <cstring>

namespace msh = mir::shell;
namespace mc = mir::compositor;
namespace mi = mir::input;
namespace ms = mir::surfaces;

msh::Surface::Surface(
    std::shared_ptr<SurfaceBuilder> const& builder,
    std::weak_ptr<ms::Surface> const& surface,
    std::function<void(std::weak_ptr<ms::Surface>)> const& destroyer,
    shell::SurfaceCreationParameters const& params,
    frontend::SurfaceId id,
    std::shared_ptr<events::EventSink> const& sink)
  : builder(builder),
    surface(builder->create_surface(params)),
    id(id),
    event_sink(sink),
    type_value(mir_surface_type_normal),
    state_value(mir_surface_state_restored)
{
    (void) surface;
    (void) destroyer;
}

msh::Surface::Surface(
    std::shared_ptr<SurfaceBuilder> const& builder,
    std::weak_ptr<ms::Surface> const& surface,
    std::function<void(std::weak_ptr<ms::Surface>)> const& destroyer,
    shell::SurfaceCreationParameters const& params)
  : builder(builder),
    surface(builder->create_surface(params)),
    id(),
    event_sink(),
    type_value(mir_surface_type_normal),
    state_value(mir_surface_state_restored)
{
    (void) surface;
    (void) destroyer;
}

msh::Surface::~Surface()
{
    if (surface.lock())
    {
        destroy();
    }
}

void msh::Surface::hide()
{
    if (auto const& s = surface.lock())
    {
        s->set_hidden(true);
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

void msh::Surface::show()
{
    if (auto const& s = surface.lock())
    {
        s->set_hidden(false);
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

bool msh::Surface::visible()
{
    if (auto const& s = surface.lock())
    {
        return s->should_be_rendered();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

void msh::Surface::destroy()
{
    builder->destroy_surface(surface);
}

void msh::Surface::force_requests_to_complete()
{
    if (auto const& s = surface.lock())
    {
        s->force_requests_to_complete();
    }
}

mir::geometry::Size msh::Surface::size() const
{
    if (auto const& s = surface.lock())
    {
        return s->size();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

void msh::Surface::move_to(geometry::Point const& p)
{
    if (auto const& s = surface.lock())
    {
        s->move_to(p);
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

mir::geometry::Point msh::Surface::top_left() const
{
    if (auto const& s = surface.lock())
    {
        return s->top_left();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

std::string msh::Surface::name() const
{
    if (auto const& s = surface.lock())
    {
        return s->name();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

mir::geometry::PixelFormat msh::Surface::pixel_format() const
{
    if (auto const& s = surface.lock())
    {
        return s->pixel_format();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

std::shared_ptr<mc::Buffer> msh::Surface::advance_client_buffer()
{
    if (auto const& s = surface.lock())
    {
        return s->advance_client_buffer();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

void msh::Surface::allow_framedropping(bool allow)
{
    if (auto const& s = surface.lock())
    {
        s->allow_framedropping(allow);
    }
}
 
void msh::Surface::with_most_recent_buffer_do(
    std::function<void(mc::Buffer&)> const& exec)
{
    if (auto const& s = surface.lock())
    {
        auto buf = s->compositor_buffer();
        exec(*buf);
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

bool msh::Surface::supports_input() const
{
    if (auto const& s = surface.lock())
    {
        return s->supports_input();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

int msh::Surface::client_input_fd() const
{
    if (auto const& s = surface.lock())
    {
        return s->client_input_fd();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

int msh::Surface::server_input_fd() const
{
    if (auto const& s = surface.lock())
    {
        return s->server_input_fd();
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
    }
}

int msh::Surface::configure(MirSurfaceAttrib attrib, int value)
{
    int result = 0;
    bool allow_dropping = false;
    /*
     * TODO: In future, query the shell implementation for the subset of
     *       attributes/types it implements.
     */
    switch (attrib)
    {
    case mir_surface_attrib_type:
        if (!set_type(static_cast<MirSurfaceType>(value)))
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface "
                                                   "type."));
        result = type();
        break;
    case mir_surface_attrib_state:
        if (value != mir_surface_state_unknown &&
            !set_state(static_cast<MirSurfaceState>(value)))
            BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface state."));
        result = state();
        break;
    case mir_surface_attrib_swapinterval:
        allow_dropping = (value == 0);
        allow_framedropping(allow_dropping);
        result = value;
        break;
    default:
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface "
                                               "attribute."));
        break;
    }

    return result;
}

MirSurfaceType msh::Surface::type() const
{
    return type_value;
}

bool msh::Surface::set_type(MirSurfaceType t)
{
    bool valid = false;

    if (t >= 0 && t < mir_surface_type_arraysize_)
    {
        type_value = t;
        valid = true;
    }

    return valid;
}

MirSurfaceState msh::Surface::state() const
{
    return state_value;
}

bool msh::Surface::set_state(MirSurfaceState s)
{
    bool valid = false;

    if (s > mir_surface_state_unknown &&
        s < mir_surface_state_arraysize_)
    {
        state_value = s;
        valid = true;

        notify_change(mir_surface_attrib_state, s);
    }

    return valid;
}

void msh::Surface::notify_change(MirSurfaceAttrib attrib, int value)
{
    if (event_sink)
    {
        MirEvent e;

        // This memset is not really required. However it does avoid some
        // harmless uninitialized memory reads that valgrind will complain
        // about, due to gaps in MirEvent.
        memset(&e, 0, sizeof e);

        e.type = mir_event_type_surface;
        e.surface.id = id.as_value();
        e.surface.attrib = attrib;
        e.surface.value = value;

        event_sink->handle_event(e);
    }
}

void msh::Surface::take_input_focus(std::shared_ptr<msh::InputTargeter> const& targeter)
{
    auto s = surface.lock();
    if (s)
        targeter->focus_changed(s);
    else
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
}
