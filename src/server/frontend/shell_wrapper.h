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

#ifndef MIR_FRONTEND_SHELL_WRAPPER_H_
#define MIR_FRONTEND_SHELL_WRAPPER_H_

#include "mir/frontend/shell.h"

namespace mir
{
namespace frontend
{
class ShellWrapper : public Shell
{
public:
    explicit ShellWrapper(std::shared_ptr<Shell> const& wrapped) :
        wrapped(wrapped) {}

    virtual ~ShellWrapper() = default;

    std::shared_ptr<Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<EventSink> const& sink) override;

    void close_session(std::shared_ptr<Session> const& session)  override;

    SurfaceId create_surface_for(
        std::shared_ptr<Session> const& session,
        scene::SurfaceCreationParameters const& params) override;

    void handle_surface_created(std::shared_ptr<Session> const& session) override;

    std::shared_ptr<PromptSession> start_prompt_session_for(
        std::shared_ptr<Session> const& session,
        scene::PromptSessionCreationParameters const& params) override;

    void add_prompt_provider_process_for(
        std::shared_ptr<PromptSession> const& prompt_session,
        pid_t process_id) override;

    void add_prompt_provider_for(
        std::shared_ptr<PromptSession> const& prompt_session,
        std::shared_ptr<Session> const& session) override;

    void stop_prompt_session(
        std::shared_ptr<PromptSession> const& prompt_session) override;

protected:
    std::shared_ptr<Shell> const wrapped;
};
}
}

#endif /* MIR_FRONTEND_SHELL_WRAPPER_H_ */
