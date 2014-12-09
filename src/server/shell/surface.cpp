/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir/shell/surface.h"

namespace mir { namespace shell {

Surface::Surface(std::shared_ptr<scene::Surface> const& impl)
    : surface(impl)
{
}

Surface::~Surface()
{
}

std::shared_ptr<mir::input::InputChannel> Surface::input_channel() const
{
    return surface->input_channel();
}

mir::input::InputReceptionMode Surface::reception_mode() const
{
    return surface->reception_mode();
}

std::string Surface::name() const
{
    return surface->name();
}

geometry::Point Surface::top_left() const
{
    return surface->top_left();
}

geometry::Size Surface::client_size() const
{
    return surface->client_size();
}

geometry::Size Surface::size() const
{
    return surface->size();
}

geometry::Rectangle Surface::input_bounds() const
{
    return surface->input_bounds();
}

bool Surface::input_area_contains(mir::geometry::Point const& p) const
{
    return surface->input_area_contains(p);
}

std::unique_ptr<graphics::Renderable> Surface::compositor_snapshot(void const* id) const
{
    return surface->compositor_snapshot(id);
}

float Surface::alpha() const
{
    return surface->alpha();
}

MirSurfaceType Surface::type() const
{
    return surface->type();
}

MirSurfaceState Surface::state() const
{
    return surface->state();
}

void Surface::hide()
{
    surface->hide();
}

void Surface::show()
{
    surface->show();
}

void Surface::move_to(geometry::Point const& p)
{
    surface->move_to(p);
}

void Surface::take_input_focus(std::shared_ptr<shell::InputTargeter> const& t)
{
    surface->take_input_focus(t);
}

void Surface::set_input_region(std::vector<geometry::Rectangle> const& r)
{
    surface->set_input_region(r);
}

void Surface::allow_framedropping(bool b)
{
    surface->allow_framedropping(b);
}

void Surface::resize(geometry::Size const& s)
{
    surface->resize(s);
}

void Surface::set_transformation(glm::mat4 const& t)
{
    surface->set_transformation(t);
}

void Surface::set_alpha(float a)
{
    surface->set_alpha(a);
}

void Surface::set_orientation(MirOrientation orientation)
{
    surface->set_orientation(orientation);
}

void Surface::force_requests_to_complete()
{
    surface->force_requests_to_complete();
}

void Surface::add_observer(std::shared_ptr<scene::SurfaceObserver> const& ob)
{
    surface->add_observer(ob);
}

void Surface::remove_observer(std::weak_ptr<scene::SurfaceObserver> const& ob)
{
    surface->remove_observer(ob);
}

void Surface::set_reception_mode(input::InputReceptionMode mode)
{
    surface->set_reception_mode(mode);
}

void Surface::consume(MirEvent const& e)
{
    surface->consume(e);
}

void Surface::set_cursor_image(std::shared_ptr<graphics::CursorImage> const& i)
{
    surface->set_cursor_image(i);
}

std::shared_ptr<graphics::CursorImage> Surface::cursor_image() const
{
    return surface->cursor_image();
}

void Surface::request_client_surface_close()
{
    surface->request_client_surface_close();
}

MirPixelFormat Surface::pixel_format() const
{
    return surface->pixel_format();
}

void Surface::swap_buffers(graphics::Buffer* old_buffer,
                           std::function<void(graphics::Buffer*)> callback)
{
    surface->swap_buffers(old_buffer, callback);
}

bool Surface::supports_input() const
{
    return surface->supports_input();
}

int Surface::client_input_fd() const
{
    return surface->client_input_fd();
}

int Surface::configure(MirSurfaceAttrib a, int v)
{
    return surface->configure(a, v);
}

int Surface::query(MirSurfaceAttrib a)
{
    return surface->query(a);
}

void Surface::with_most_recent_buffer_do(
    std::function<void(graphics::Buffer&)> const& callback)
{
    surface->with_most_recent_buffer_do(callback);
}

}} // namespace mir::shell
