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
 * Authored By: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/frontend/session.h"
#include "mir/shell/default_focus_mechanism.h"
#include "mir/shell/input_targeter.h"
#include "mir/shell/session.h"
#include "mir/shell/surface.h"
#include "mir/shell/surface_controller.h"

namespace mf = mir::frontend;
namespace msh = mir::shell;

msh::DefaultFocusMechanism::DefaultFocusMechanism(std::shared_ptr<msh::InputTargeter> const& input_targeter,
                                                  std::shared_ptr<msh::SurfaceController> const& surface_controller)
  : input_targeter(input_targeter),
    surface_controller(surface_controller)
{
}

void msh::DefaultFocusMechanism::set_focus_to(std::shared_ptr<Session> const& focus_session)
{
    std::lock_guard<std::mutex> lg(focus_lock);

    if (currently_focused_session)
    {
        currently_focused_session->relinquish_focus();
    }

    // TODO: This path should be encapsulated in a seperate clear_focus message
    if (!focus_session)
    {
        input_targeter->focus_cleared();
    }
    focus_session->receive_focus(input_targeter, surface_controller);
    currently_focused_session = focus_session;
}
