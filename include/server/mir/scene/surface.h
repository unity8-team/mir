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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_SURFACE_H_
#define MIR_SCENE_SURFACE_H_

#include "mir/graphics/renderable.h"
#include "mir/input/surface.h"
#include "mir/shell/surface.h"

namespace mir
{
namespace input { class InputChannel; }

namespace scene
{
class Surface :
    public graphics::Renderable,
    public input::Surface,
    public shell::Surface
{
public:
    // resolve ambiguous member function names
    std::string name() const = 0;
    geometry::Size size() const = 0;
    geometry::Point top_left() const = 0;
    float alpha() const = 0;

    // member functions that don't exist in base classes
    virtual std::shared_ptr<input::InputChannel> input_channel() const = 0;
};
}
}

#endif // MIR_SCENE_SURFACE_H_
