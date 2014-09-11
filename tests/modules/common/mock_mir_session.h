/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MOCK_MIR_SCENE_SESSION_H
#define MOCK_MIR_SCENE_SESSION_H

#include <mir/scene/session.h>
#include <mir/graphics/display_configuration.h>
#include <mir/scene/surface_creation_parameters.h>
#include <gmock/gmock.h>

#include <string>

namespace mir {
namespace scene {

struct MockSession : public Session
{
    MockSession() {}
    MockSession(std::string const& sessionName, pid_t processId) 
        : m_sessionName(sessionName), m_sessionId(processId)
    {}

    std::string name() const override
    {
        return m_sessionName;
    }

    pid_t process_id() const override
    {
        return m_sessionId;
    }

    MOCK_METHOD0(force_requests_to_complete, void());

    MOCK_CONST_METHOD0(default_surface, std::shared_ptr<Surface>());
    MOCK_CONST_METHOD1(get_surface, std::shared_ptr<frontend::Surface>(frontend::SurfaceId));

    MOCK_METHOD1(take_snapshot, void(SnapshotCallback const&));
    MOCK_METHOD1(set_lifecycle_state, void(MirLifecycleState));
    MOCK_METHOD1(create_surface, frontend::SurfaceId(SurfaceCreationParameters const&));
    MOCK_METHOD1(destroy_surface, void (frontend::SurfaceId));

    MOCK_METHOD0(hide, void());
    MOCK_METHOD0(show, void());
    MOCK_METHOD1(send_display_config, void(graphics::DisplayConfiguration const&));
    MOCK_METHOD3(configure_surface, int(frontend::SurfaceId, MirSurfaceAttrib, int));

    void start_prompt_session() override {};
    void stop_prompt_session() override {};

private:
    std::string m_sessionName;
    pid_t m_sessionId;
};

} // namespace scene
} // namespace mir

#endif // MOCK_MIR_SCENE_SESSION_H
