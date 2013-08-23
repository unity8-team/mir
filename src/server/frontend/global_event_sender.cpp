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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/frontend/global_event_sender.h"
#include "mir/shell/session_container.h"
#include "mir/shell/session.h"

namespace mf=mir::frontend;
namespace mg=mir::graphics;
namespace msh=mir::shell;

mf::GlobalEventSender::GlobalEventSender(std::shared_ptr<shell::SessionContainer> const& session_container)
    : sessions(session_container)
{
}

void mf::GlobalEventSender::handle_event(MirEvent const&)
{ 
    //TODO, no driving test cases, although messages like 'server shutdown' could go here
}

void mf::GlobalEventSender::handle_lifecycle_event(MirLifecycleState)
{
    // Lifecycle events are per application session, never global
}

void mf::GlobalEventSender::handle_display_config_change(mg::DisplayConfiguration const& config)
{
    sessions->for_each([&config](std::shared_ptr<msh::Session> const& session)
    {
        session->send_display_config(config);
    });
}
