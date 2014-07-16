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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "shell_wrapper.h"

namespace mf = mir::frontend;

std::shared_ptr<mf::Session> mf::ShellWrapper::open_session(
    pid_t client_pid,
    std::string const& name,
    std::shared_ptr<EventSink> const& sink)
{
    return wrapped->open_session(client_pid, name,sink);
}

void mf::ShellWrapper::close_session(std::shared_ptr<Session> const& session)
{
    wrapped->close_session(session);
}

mf::SurfaceId mf::ShellWrapper::create_surface_for(
    std::shared_ptr<Session> const& session,
    scene::SurfaceCreationParameters const& params)
{
    return wrapped->create_surface_for(session, params);
}

void mf::ShellWrapper::handle_surface_created(
    std::shared_ptr<Session> const& session)
{
    wrapped->handle_surface_created(session);
}

std::shared_ptr<mf::PromptSession> mf::ShellWrapper::start_prompt_session_for(
    std::shared_ptr<Session> const& session,
    scene::PromptSessionCreationParameters const& params)
{
    return wrapped->start_prompt_session_for(session, params);
}

void mf::ShellWrapper::add_prompt_provider_for(
    std::shared_ptr<PromptSession> const& prompt_session,
    std::shared_ptr<Session> const& session)
{
    wrapped->add_prompt_provider_for(prompt_session, session);
}

void mf::ShellWrapper::stop_prompt_session(
    std::shared_ptr<PromptSession> const& prompt_session)
{
    wrapped->stop_prompt_session(prompt_session);
}
