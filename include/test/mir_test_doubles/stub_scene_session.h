/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#ifndef MIR_TEST_DOUBLES_STUB_SCENE_SESSION_H_
#define MIR_TEST_DOUBLES_STUB_SCENE_SESSION_H_

#include "mir/scene/session.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubSceneSession : public scene::Session
{
    frontend::SurfaceId create_surface(shell::SurfaceCreationParameters const& /* params */) override
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
    std::string name() const override
    {
        return std::string();
    }
    pid_t process_id() const override
    {
        return -1;
    }

    void force_requests_to_complete() override
    {
    }
    void hide() override
    {
    }
    void show() override
    {
    }

    void send_display_config(graphics::DisplayConfiguration const&) override
    {
    }

    void take_snapshot(shell::SnapshotCallback const&) override
    {
    }

    std::shared_ptr<scene::Surface> default_surface() const override
    {
        return std::shared_ptr<scene::Surface>();
    }

    void set_lifecycle_state(MirLifecycleState /*state*/)
    {
    }
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_STUB_SCENE_SESSION_H_

