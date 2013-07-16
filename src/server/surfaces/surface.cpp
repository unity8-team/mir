/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/surfaces/surface.h"
#include "mir/surfaces/surface_info.h"
#include "mir/input/surface_info.h"
#include "mir/surfaces/buffer_stream.h"
#include "mir/input/input_channel.h"
#include "mir/compositor/buffer.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <cassert>

#include <glm/gtc/matrix_transform.hpp>

namespace mc = mir::compositor;
namespace ms = mir::surfaces;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

ms::Surface::Surface(
    std::shared_ptr<ms::SurfaceInfoController> const& basic_info,
    std::shared_ptr<mi::SurfaceInfoController> const& input_info,
    std::shared_ptr<BufferStream> buffer_stream,
    std::shared_ptr<input::InputChannel> const& input_channel,
    std::function<void()> const& change_callback) :
    basic_info(basic_info),
    input_info(input_info),
    buffer_stream(buffer_stream),
    server_input_channel(input_channel),
    transformation_dirty(true),
    alpha_value(1.0f),
    is_hidden(false),
    buffer_count(0),
    notify_change(change_callback)
{
    assert(buffer_stream);
    assert(change_callback);
}

void ms::Surface::force_requests_to_complete()
{
    buffer_stream->force_requests_to_complete();
}

ms::Surface::~Surface()
{
}

std::string const& ms::Surface::name() const
{
    return basic_info->name();
}

void ms::Surface::move_to(geometry::Point const& top_left)
{
    basic_info->move_to(top_left);
    transformation_dirty = true;
    notify_change();
}

void ms::Surface::set_rotation(float degrees, glm::vec3 const& axis)
{
    rotation_matrix = glm::rotate(glm::mat4{1.0f}, degrees, axis);
    transformation_dirty = true;
    notify_change();
}

void ms::Surface::set_alpha(float alpha_v)
{
    alpha_value = alpha_v;
    notify_change();
}

void ms::Surface::set_hidden(bool hide)
{
    is_hidden = hide;
    notify_change();
}

geom::Point ms::Surface::top_left() const
{
    return basic_info->size_and_position().top_left;
}

mir::geometry::Size ms::Surface::size() const
{
    return basic_info->size_and_position().size;
}

std::shared_ptr<ms::GraphicRegion> ms::Surface::graphic_region() const
{
    return compositor_buffer();
}

const glm::mat4& ms::Surface::transformation() const
{
    auto rect = basic_info->size_and_position();
    auto sz = rect.size;

    if (transformation_dirty || transformation_size != sz)
    {
        auto pt = rect.top_left;
        const glm::vec3 top_left_vec{pt.x.as_int(),
                                     pt.y.as_int(),
                                     0.0f};
        const glm::vec3 size_vec{sz.width.as_uint32_t(),
                                 sz.height.as_uint32_t(),
                                 0.0f};

        /* Get the center of the renderable's area */
        const glm::vec3 center_vec{top_left_vec + 0.5f * size_vec};

        /*
         * Every renderable is drawn using a 1x1 quad centered at 0,0.
         * We need to transform and scale that quad to get to its final position
         * and size.
         *
         * 1. We scale the quad vertices (from 1x1 to wxh)
         * 2. We move the quad to its final position. Note that because the quad
         *    is centered at (0,0), we need to translate by center_vec, not
         *    top_left_vec.
         */
        glm::mat4 pos_size_matrix;
        pos_size_matrix = glm::translate(pos_size_matrix, center_vec);
        pos_size_matrix = glm::scale(pos_size_matrix, size_vec);

        // Rotate, then scale, then translate
        transformation_matrix = pos_size_matrix * rotation_matrix;
        transformation_size = sz;
        transformation_dirty = false;
    }

    return transformation_matrix;
}

float ms::Surface::alpha() const
{
    return alpha_value;
}

bool ms::Surface::should_be_rendered() const
{
    return !is_hidden && (buffer_count > 1);
}

//note: not sure the surface should be aware of pixel format. might be something that the
//texture (which goes to compositor should be aware of though
//todo: kdub remove
geom::PixelFormat ms::Surface::pixel_format() const
{
    return buffer_stream->get_stream_pixel_format();
}

std::shared_ptr<mg::Buffer> ms::Surface::advance_client_buffer()
{
    if (buffer_count < 2)
        buffer_count++;

    notify_change();
    return buffer_stream->secure_client_buffer();
}

void ms::Surface::allow_framedropping(bool allow)
{
    buffer_stream->allow_framedropping(allow);
}

std::shared_ptr<mg::Buffer> ms::Surface::compositor_buffer() const
{
    return buffer_stream->lock_back_buffer();
}

void ms::Surface::flag_for_render()
{
    buffer_count = 2;
}

bool ms::Surface::supports_input() const
{
    if (server_input_channel)
        return true;
    return false;
}

int ms::Surface::client_input_fd() const
{
    if (!supports_input())
        BOOST_THROW_EXCEPTION(std::logic_error("Surface does not support input"));
    return server_input_channel->client_fd();
}

std::shared_ptr<mi::InputChannel> ms::Surface::input_channel() const
{
    return server_input_channel;
}

void ms::Surface::set_input_region(std::vector<geom::Rectangle> const& input_rectangles)
{
    input_info->set_input_region(input_rectangles);
}
