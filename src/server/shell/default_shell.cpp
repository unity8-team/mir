/*
 * Copyright © 2012 Canonical Ltd.
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

#include "default_shell.h"
#include "mir/shell/application_session.h"
#include "session_container.h"
#include "mir/shell/surface_factory.h"
#include "mir/shell/focus_setter.h"
#include "mir/shell/session.h"
#include "mir/shell/surface.h"
#include "mir/shell/session_listener.h"
#include "session_event_sink.h"

#include <boost/throw_exception.hpp>

#include <memory>
#include <cassert>
#include <algorithm>
#include <stdexcept>

namespace mf = mir::frontend;
namespace msh = mir::shell;

msh::DefaultShell::DefaultShell(std::shared_ptr<msh::SurfaceFactory> const& surface_factory,
    std::shared_ptr<msh::FocusSetter> const& focus_setter,
    std::shared_ptr<msh::SnapshotStrategy> const& snapshot_strategy,
    std::shared_ptr<msh::SessionEventSink> const& session_event_sink,
    std::shared_ptr<msh::SessionListener> const& session_listener) :
    surface_factory(surface_factory),
    focus_setter(focus_setter),
    snapshot_strategy(snapshot_strategy),
    session_event_sink(session_event_sink),
    session_listener(session_listener)
{
    assert(surface_factory);
    assert(focus_setter);
    assert(session_listener);
}

msh::DefaultShell::~DefaultShell()
{
    /*
     * Close all open sessions. We need to do this manually here
     * to break the cyclic dependency between msh::Session
     * and mi::*, since our implementations
     * of these interfaces keep strong references to each other.
     * TODO: Investigate other solutions (e.g. weak_ptr)
     */
    std::unique_lock<std::mutex> lg(mutex);
    auto sessions_copy = sessions;
    for (auto& session : sessions_copy)
        close_session_locked(lg, session);
}

std::shared_ptr<mf::Session> msh::DefaultShell::open_session(std::string const& name,
                                                std::shared_ptr<mf::EventSink> const& sender)
{
    std::shared_ptr<msh::Session> new_session =
        std::make_shared<msh::ApplicationSession>(
            surface_factory, name, snapshot_strategy, session_listener, sender);

    std::unique_lock<std::mutex> lg(mutex);
    sessions.push_back(new_session);
    
    session_listener->starting(new_session);

    set_focus_to_locked(lg, new_session);

    return new_session;
}

inline void msh::DefaultShell::set_focus_to_locked(std::unique_lock<std::mutex> const&, std::shared_ptr<Session> const& shell_session)
{
    auto old_focus = focus_application.lock();

    focus_application = shell_session;

    focus_setter->set_focus_to(shell_session);
    if (shell_session)
    {
        session_event_sink->handle_focus_change(shell_session);
        session_listener->focused(shell_session);
    }
    else
    {
        session_event_sink->handle_no_focus();
        session_listener->unfocused();
    }
}

void msh::DefaultShell::set_focus_to(std::shared_ptr<Session> const& shell_session)
{
    std::unique_lock<std::mutex> lg(mutex);
    set_focus_to_locked(lg, shell_session);
}

void msh::DefaultShell::close_session(std::shared_ptr<mf::Session> const& session)
{
    std::unique_lock<std::mutex> lg(mutex);
    close_session_locked(lg, std::dynamic_pointer_cast<Session>(session));
}

void msh::DefaultShell::close_session_locked(std::unique_lock<std::mutex> const& lg, std::shared_ptr<msh::Session> const& session)
{

    session_event_sink->handle_session_stopping(session);
    session_listener->stopping(session);

    auto it = std::find(sessions.begin(), sessions.end(), session);
    if (it == sessions.end())
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid session"));
    sessions.erase(it);

    if (sessions.empty())
        set_focus_to_locked(lg, std::shared_ptr<msh::Session>());
    else
        set_focus_to_locked(lg, *sessions.rbegin());

}

void msh::DefaultShell::focus_next()
{
    std::unique_lock<std::mutex> lock(mutex);
    auto focus = focus_application.lock();

    if (sessions.empty())
    {
        focus = std::shared_ptr<msh::Session>();
    }
    else if (!focus)
    {
        focus = *sessions.rbegin();
    }
    else
    {
        auto it = std::find(sessions.begin(), sessions.end(), focus);
        it++;
        if (it == sessions.end())
            it = sessions.begin();
        focus = *it;
    }

    set_focus_to_locked(lock, focus);
}

std::weak_ptr<msh::Session> msh::DefaultShell::focussed_application() const
{
    return focus_application;
}

// TODO: We use this to work around the lack of a SessionMediator-like object for internal clients.
// we could have an internal client mediator which acts as a factory for internal clients, taking responsibility
// for invoking handle_surface_created.
mf::SurfaceId msh::DefaultShell::create_surface_for(std::shared_ptr<mf::Session> const& session,
    msh::SurfaceCreationParameters const& params)
{
    auto shell_session = std::dynamic_pointer_cast<Session>(session);
    auto id = shell_session->create_surface(params);
    
    handle_surface_created(session);

    return id;
}

void msh::DefaultShell::handle_surface_created(std::shared_ptr<mf::Session> const& session)
{
    auto shell_session = std::dynamic_pointer_cast<Session>(session);

    set_focus_to(shell_session);
}
                                                 
void msh::DefaultShell::for_each(std::function<void(std::shared_ptr<Session> const&)> f) const
{
    std::unique_lock<std::mutex> lg(mutex);
    for (auto& session : sessions)
        f(session);
}
