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

#ifndef MIR_TEST_DOUBLES_STUB_SESSION_H_
#define MIR_TEST_DOUBLES_STUB_SESSION_H_

#include "mir/frontend/session.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubSession : public frontend::Session
{
    frontend::SurfaceId create_surface(scene::SurfaceCreationParameters const& /* params */) override
    {
        return frontend::SurfaceId{0};
    }
    void destroy_surface(frontend::SurfaceId /* surface */) override
    {
    }
    std::shared_ptr<frontend::Surface> get_surface(frontend::SurfaceId /* surface */) const override
    {
        return std::shared_ptr<frontend::Surface>();
    }
    std::shared_ptr<frontend::BufferStream> get_buffer_stream(frontend::BufferStreamId /* id */) const override
    {
        return std::shared_ptr<frontend::BufferStream>();
    }
    std::string name() const override
    {
        return std::string();
    }
    void hide() override
    {
    }
    void show() override
    {
    }
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_STUB_SESSION_H_
