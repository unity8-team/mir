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


#ifndef MIR_FRONTEND_SURFACE_H_
#define MIR_FRONTEND_SURFACE_H_

#include "mir/geometry/pixel_format.h"
#include "mir/geometry/point.h"
#include "mir/geometry/size.h"
#include "mir_toolkit/common.h"

#include <memory>

namespace mir
{
namespace compositor
{
class Buffer;
}
namespace input
{
class InputChannel;
}
namespace surfaces
{
class Surface;
}
namespace shell
{
class InputTargeter;
}
namespace frontend
{

class Surface
{
public:

    virtual ~Surface() {}

    virtual void destroy() = 0;
    virtual void force_requests_to_complete() = 0;

    virtual geometry::Size size() const = 0;
    virtual geometry::PixelFormat pixel_format() const = 0;

    virtual std::shared_ptr<compositor::Buffer> advance_client_buffer() = 0;

    virtual bool supports_input() const = 0;
    virtual int client_input_fd() const = 0;

    virtual int configure(MirSurfaceAttrib attrib, int value) = 0;


    //hack
    virtual void hide() = 0;
    virtual void show() = 0;
    virtual bool visible() = 0;
    virtual std::string name() const = 0;
    virtual void move_to(geometry::Point const& top_left) = 0;
    virtual geometry::Point top_left() const = 0;
    virtual void with_most_recent_buffer_do(
        std::function<void(compositor::Buffer&)> const& exec) = 0;
    virtual int server_input_fd() const = 0;
    virtual MirSurfaceType type() const = 0;
    virtual MirSurfaceState state() const = 0;
    virtual void take_input_focus(std::shared_ptr<shell::InputTargeter> const& targeter) = 0;
    virtual std::weak_ptr<surfaces::Surface> stack_surface() const = 0;
protected:
    Surface() = default;
    Surface(Surface const&) = delete;
    Surface& operator=(Surface const&) = delete;
};

}
}


#endif /* MIR_FRONTEND_SURFACE_H_ */
