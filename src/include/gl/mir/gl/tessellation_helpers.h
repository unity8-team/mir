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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GL_TESSELLATION_HELPERS_H_
#define MIR_GL_TESSELLATION_HELPERS_H_
#include "mir/gl/primitive.h"
#include "mir/geometry/displacement.h"

namespace mir
{
namespace graphics { class Renderable; }
namespace gl
{

Primitive tessellate_renderable_into_rectangle(
    graphics::Renderable const& renderable, geometry::Displacement const& offset);

}
}
#endif /* MIR_GL_TESSELLATION_HELPERS_H_ */
