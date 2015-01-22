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

#include "mir/scene/surface_wrapper.h"

namespace mir { namespace scene {

SurfaceWrapper::SurfaceWrapper(std::shared_ptr<Surface> const& impl)
    : raw_surface(impl)
{
}

SurfaceWrapper::~SurfaceWrapper()
{
}

std::shared_ptr<mir::input::InputChannel> SurfaceWrapper::input_channel() const
{
    return raw_surface->input_channel();
}

mir::input::InputReceptionMode SurfaceWrapper::reception_mode() const
{
    return raw_surface->reception_mode();
}

std::string SurfaceWrapper::name() const
{
    return raw_surface->name();
}

geometry::Point SurfaceWrapper::top_left() const
{
    return raw_surface->top_left();
}

geometry::Size SurfaceWrapper::client_size() const
{
    return raw_surface->client_size();
}

geometry::Size SurfaceWrapper::size() const
{
    return raw_surface->size();
}

geometry::Rectangle SurfaceWrapper::input_bounds() const
{
    return raw_surface->input_bounds();
}

bool SurfaceWrapper::input_area_contains(mir::geometry::Point const& p) const
{
    return raw_surface->input_area_contains(p);
}

std::unique_ptr<graphics::Renderable> SurfaceWrapper::compositor_snapshot(void const* id) const
{
    return raw_surface->compositor_snapshot(id);
}

float SurfaceWrapper::alpha() const
{
    return raw_surface->alpha();
}

MirSurfaceType SurfaceWrapper::type() const
{
    return raw_surface->type();
}

MirSurfaceState SurfaceWrapper::state() const
{
    return raw_surface->state();
}

void SurfaceWrapper::hide()
{
    raw_surface->hide();
}

void SurfaceWrapper::show()
{
    raw_surface->show();
}

bool SurfaceWrapper::visible() const
{
    return raw_surface->visible();
}

void SurfaceWrapper::move_to(geometry::Point const& p)
{
    raw_surface->move_to(p);
}

void SurfaceWrapper::take_input_focus(std::shared_ptr<shell::InputTargeter> const& t)
{
    raw_surface->take_input_focus(t);
}

void SurfaceWrapper::set_input_region(std::vector<geometry::Rectangle> const& r)
{
    raw_surface->set_input_region(r);
}

void SurfaceWrapper::allow_framedropping(bool b)
{
    raw_surface->allow_framedropping(b);
}

void SurfaceWrapper::resize(geometry::Size const& s)
{
    raw_surface->resize(s);
}

void SurfaceWrapper::set_transformation(glm::mat4 const& t)
{
    raw_surface->set_transformation(t);
}

void SurfaceWrapper::set_alpha(float a)
{
    raw_surface->set_alpha(a);
}

void SurfaceWrapper::set_orientation(MirOrientation orientation)
{
    raw_surface->set_orientation(orientation);
}

void SurfaceWrapper::force_requests_to_complete()
{
    raw_surface->force_requests_to_complete();
}

void SurfaceWrapper::add_observer(std::shared_ptr<SurfaceObserver> const& ob)
{
    raw_surface->add_observer(ob);
}

void SurfaceWrapper::remove_observer(std::weak_ptr<SurfaceObserver> const& ob)
{
    raw_surface->remove_observer(ob);
}

void SurfaceWrapper::set_reception_mode(input::InputReceptionMode mode)
{
    raw_surface->set_reception_mode(mode);
}

void SurfaceWrapper::consume(MirEvent const& e)
{
    raw_surface->consume(e);
}

void SurfaceWrapper::set_cursor_image(std::shared_ptr<graphics::CursorImage> const& i)
{
    raw_surface->set_cursor_image(i);
}

std::shared_ptr<graphics::CursorImage> SurfaceWrapper::cursor_image() const
{
    return raw_surface->cursor_image();
}

void SurfaceWrapper::request_client_surface_close()
{
    raw_surface->request_client_surface_close();
}

MirPixelFormat SurfaceWrapper::pixel_format() const
{
    return raw_surface->pixel_format();
}

void SurfaceWrapper::swap_buffers(graphics::Buffer* old_buffer,
                           std::function<void(graphics::Buffer*)> callback)
{
    raw_surface->swap_buffers(old_buffer, callback);
}

bool SurfaceWrapper::supports_input() const
{
    return raw_surface->supports_input();
}

int SurfaceWrapper::client_input_fd() const
{
    return raw_surface->client_input_fd();
}

int SurfaceWrapper::configure(MirSurfaceAttrib a, int v)
{
    return raw_surface->configure(a, v);
}

int SurfaceWrapper::query(MirSurfaceAttrib a)
{
    return raw_surface->query(a);
}

void SurfaceWrapper::with_most_recent_buffer_do(
    std::function<void(graphics::Buffer&)> const& callback)
{
    raw_surface->with_most_recent_buffer_do(callback);
}

std::shared_ptr<Surface> SurfaceWrapper::parent() const
{
    return raw_surface->parent();
}

}} // namespace mir::scene
