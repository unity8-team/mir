/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 *              Kevin DuBois <kevin.dubois@canonical.com>
 */
#include "mir/gl/tessellation_helpers.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/buffer.h"

namespace mg = mir::graphics;
namespace mgl = mir::gl;
namespace geom = mir::geometry;
mgl::Primitive mgl::tessellate_renderable_into_rectangle(
    mg::Renderable const& renderable, geom::Displacement const& offset)
{
    auto const& buf_size = renderable.buffer()->size();
    auto rect = renderable.screen_position();
    rect.top_left = rect.top_left - offset;
    GLfloat left = rect.top_left.x.as_int();
    GLfloat right = left + rect.size.width.as_int();
    GLfloat top = rect.top_left.y.as_int();
    GLfloat bottom = top + rect.size.height.as_int();

    mgl::Primitive rectangle;
    rectangle.tex_id = 0;
    rectangle.type = GL_TRIANGLE_STRIP;

    GLfloat tex_right = static_cast<GLfloat>(rect.size.width.as_int()) /
                        buf_size.width.as_int();
    GLfloat tex_bottom = static_cast<GLfloat>(rect.size.height.as_int()) /
                         buf_size.height.as_int();

    auto& vertices = rectangle.vertices;
    vertices[0] = {{left,  top,    0.0f}, {0.0f,      0.0f}};
    vertices[1] = {{left,  bottom, 0.0f}, {0.0f,      tex_bottom}};
    vertices[2] = {{right, top,    0.0f}, {tex_right, 0.0f}};
    vertices[3] = {{right, bottom, 0.0f}, {tex_right, tex_bottom}};
    return rectangle;
}
