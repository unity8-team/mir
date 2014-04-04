/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_SCENE_APPLICATION_MANAGER_H_
#define MIR_SCENE_APPLICATION_MANAGER_H_

#include "mir/frontend/surface_id.h"
#include "mir/frontend/shell.h"
#include "mir/shell/focus_controller.h"

#include <mutex>
#include <memory>
#include <vector>

namespace mir
{

namespace shell
{
class FocusSetter;
class SessionListener;
}

namespace scene
{
class SessionEventSink;
class SessionContainer;
class SnapshotStrategy;
class SurfaceCoordinator;

class SessionManager : public frontend::Shell, public shell::FocusController
{
public:
    explicit SessionManager(std::shared_ptr<SurfaceCoordinator> const& surface_coordinator,
                            std::shared_ptr<SessionContainer> const& app_container,
                            std::shared_ptr<shell::FocusSetter> const& focus_setter,
                            std::shared_ptr<SnapshotStrategy> const& snapshot_strategy,
                            std::shared_ptr<SessionEventSink> const& session_event_sink,
                            std::shared_ptr<shell::SessionListener> const& session_listener);
    virtual ~SessionManager();

    virtual std::shared_ptr<frontend::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<frontend::EventSink> const& sink);

    virtual void close_session(std::shared_ptr<frontend::Session> const& session);

    frontend::SurfaceId create_surface_for(std::shared_ptr<frontend::Session> const& session,
                                 shell::SurfaceCreationParameters const& params);

    void focus_next();
    std::weak_ptr<shell::Session> focussed_application() const;
    void set_focus_to(std::shared_ptr<shell::Session> const& focus);

    void handle_surface_created(std::shared_ptr<frontend::Session> const& session);

protected:
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

private:
    std::shared_ptr<SurfaceCoordinator> const surface_coordinator;
    std::shared_ptr<SessionContainer> const app_container;
    std::shared_ptr<shell::FocusSetter> const focus_setter;
    std::shared_ptr<SnapshotStrategy> const snapshot_strategy;
    std::shared_ptr<SessionEventSink> const session_event_sink;
    std::shared_ptr<shell::SessionListener> const session_listener;

    std::mutex mutex;
    std::weak_ptr<shell::Session> focus_application;

    void set_focus_to_locked(std::unique_lock<std::mutex> const& lock, std::shared_ptr<shell::Session> const& next_focus);
};

}
}

#endif // MIR_SCENE_APPLICATION_MANAGER_H_
