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


#ifndef MIR_SHELL_SURFACE_H_
#define MIR_SHELL_SURFACE_H_

#include "mir/shell/surface_buffer_access.h"
#include "mir/geometry/rectangle.h"
#include "mir/frontend/surface.h"

#include <string>
#include <vector>

namespace mir
{
namespace shell
{
class InputTargeter;

class Surface : public frontend::Surface, public shell::SurfaceBufferAccess
{
public:
    virtual std::string name() const = 0;
    virtual MirSurfaceType type() const = 0;
    virtual MirSurfaceState state() const = 0;
    virtual void hide() = 0;
    virtual void show() = 0;
    virtual void move_to(geometry::Point const& top_left) = 0;
    virtual geometry::Point top_left() const = 0;

    virtual void take_input_focus(std::shared_ptr<InputTargeter> const& targeter) = 0;
    virtual void set_input_region(std::vector<geometry::Rectangle> const& region) = 0;

    virtual void allow_framedropping(bool) = 0;

    virtual void resize(geometry::Size const& size) = 0;
    virtual void set_transformation(glm::mat4 const& t) = 0;

    virtual float alpha() const = 0;
    virtual void set_alpha(float alpha) = 0;
    virtual void force_requests_to_complete() = 0;
};
}
}

#endif /* MIR_SHELL_SURFACE_H_ */
