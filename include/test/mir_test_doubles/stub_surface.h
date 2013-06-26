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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_SURFACE_H_
#define MIR_TEST_DOUBLES_STUB_SURFACE_H_

#include "mir/frontend/surface.h"

namespace mir
{
namespace test
{
namespace doubles
{

class StubSurface : public frontend::Surface
{
public:
    virtual ~StubSurface() = default;

    void hide() {}
    void show() {}
    void destroy() {}
    void force_requests_to_complete() {}

    geometry::Size size() const
    {
        return geometry::Size();
    }
    geometry::PixelFormat pixel_format() const
    {
        return geometry::PixelFormat();
    }

    std::shared_ptr<compositor::Buffer> advance_client_buffer()
    {
        return std::shared_ptr<compositor::Buffer>();
    }

    virtual int configure(MirSurfaceAttrib, int)
    {
        return 0;
    }

    virtual bool supports_input() const
    {
        return false;
    }

    virtual int client_input_fd() const
    {
        return 0;
    }

    virtual bool visible() {return true;}
    virtual std::string name() const {return std::string("a");}
    virtual void move_to(geometry::Point const&) {}
    virtual geometry::Point top_left() const { return geometry::Point{geometry::X{4}, geometry::Y{3}}; }
    virtual void with_most_recent_buffer_do(std::function<void(compositor::Buffer&)> const&) {}
    virtual int server_input_fd() const { return 5;}
    virtual MirSurfaceType type() const { return mir_surface_type_normal;}
    virtual MirSurfaceState state() const { return mir_surface_state_unknown;}
    virtual void take_input_focus(std::shared_ptr<shell::InputTargeter> const&) {}
    virtual std::weak_ptr<surfaces::Surface> stack_surface() const { return std::weak_ptr<surfaces::Surface>(); }
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_STUB_SURFACE_H_
