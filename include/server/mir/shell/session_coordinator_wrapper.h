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

#ifndef MIR_SHELL_SESSION_COORDINATOR_WRAPPER_H_
#define MIR_SHELL_SESSION_COORDINATOR_WRAPPER_H_


#include "mir/scene/session_coordinator.h"

namespace mir
{
namespace shell
{
class SessionCoordinatorWrapper : public scene::SessionCoordinator
{
public:
    SessionCoordinatorWrapper(std::shared_ptr<scene::SessionCoordinator> const& wrapped);

    virtual std::shared_ptr<scene::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<frontend::EventSink> const& sink) override;

    virtual void close_session(std::shared_ptr<scene::Session> const& session) override;

    std::shared_ptr<scene::Session> successor_of(std::shared_ptr<scene::Session> const&) const override;

    void set_focus_to(std::shared_ptr<scene::Session> const& focus) override;
    void unset_focus() override;

protected:
    std::shared_ptr<scene::SessionCoordinator> const wrapped;
};
}
}

#endif /* MIR_SHELL_SESSION_COORDINATOR_WRAPPER_H_ */
