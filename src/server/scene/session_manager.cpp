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

#include "session_manager.h"
#include "application_session.h"
#include "session_container.h"
#include "mir/scene/surface.h"
#include "mir/scene/surface_coordinator.h"
#include "mir/scene/session.h"
#include "mir/scene/session_listener.h"
#include "mir/scene/prompt_session.h"
#include "mir/scene/prompt_session_manager.h"
#include "session_event_sink.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <memory>
#include <cassert>
#include <algorithm>

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

ms::SessionManager::SessionManager(std::shared_ptr<SurfaceCoordinator> const& surface_factory,
    std::shared_ptr<SessionContainer> const& container,
    std::shared_ptr<SnapshotStrategy> const& snapshot_strategy,
    std::shared_ptr<SessionEventSink> const& session_event_sink,
    std::shared_ptr<SessionListener> const& session_listener,
    std::shared_ptr<PromptSessionManager> const& prompt_session_manager) :
    surface_coordinator(surface_factory),
    app_container(container),
    snapshot_strategy(snapshot_strategy),
    session_event_sink(session_event_sink),
    session_listener(session_listener),
    prompt_session_manager(prompt_session_manager)
{
    assert(surface_factory);
    assert(container);
    assert(session_listener);
}

ms::SessionManager::~SessionManager() noexcept
{
    /*
     * Close all open sessions. We need to do this manually here
     * to break the cyclic dependency between msh::Session
     * and mi::*, since our implementations
     * of these interfaces keep strong references to each other.
     * TODO: Investigate other solutions (e.g. weak_ptr)
     */
    std::vector<std::shared_ptr<Session>> sessions;

    app_container->for_each([&](std::shared_ptr<Session> const& session)
    {
        sessions.push_back(session);
    });

    for (auto& session : sessions)
        close_session(session);
}

std::shared_ptr<ms::Session> ms::SessionManager::open_session(
    pid_t client_pid,
    std::string const& name,
    std::shared_ptr<mf::EventSink> const& sender)
{
    std::shared_ptr<Session> new_session =
        std::make_shared<ApplicationSession>(
            surface_coordinator, client_pid, name, snapshot_strategy, session_listener, sender);

    app_container->insert_session(new_session);

    session_listener->starting(new_session);

    return new_session;
}

void ms::SessionManager::set_focus_to(std::shared_ptr<Session> const& session)
{
    session_event_sink->handle_focus_change(session);
    session_listener->focused(session);
}

void ms::SessionManager::unset_focus()
{
    session_event_sink->handle_no_focus();
    session_listener->unfocused();
}

void ms::SessionManager::close_session(std::shared_ptr<Session> const& session)
{
    auto scene_session = std::dynamic_pointer_cast<Session>(session);

    scene_session->force_requests_to_complete();

    session_event_sink->handle_session_stopping(scene_session);

    prompt_session_manager->remove_session(scene_session);

    session_listener->stopping(scene_session);

    app_container->remove_session(scene_session);
}

std::shared_ptr<ms::PromptSession> ms::SessionManager::start_prompt_session_for(std::shared_ptr<Session> const& session,
    PromptSessionCreationParameters const& params)
{
    auto shell_session = std::dynamic_pointer_cast<Session>(session);

    return prompt_session_manager->start_prompt_session_for(
        shell_session, params);

}

void ms::SessionManager::add_prompt_provider_for(
    std::shared_ptr<PromptSession> const& prompt_session,
    std::shared_ptr<Session> const& session)
{
    auto scene_prompt_session = std::dynamic_pointer_cast<PromptSession>(prompt_session);
    auto scene_session = std::dynamic_pointer_cast<Session>(session);

    prompt_session_manager->add_prompt_provider(scene_prompt_session, scene_session);
}

void ms::SessionManager::stop_prompt_session(std::shared_ptr<PromptSession> const& prompt_session)
{
    auto scene_prompt_session = std::dynamic_pointer_cast<PromptSession>(prompt_session);
    prompt_session_manager->stop_prompt_session(scene_prompt_session);
}

std::shared_ptr<ms::Session> ms::SessionManager::successor_of(
    std::shared_ptr<Session> const& session) const
{
    return app_container->successor_of(session);
}
