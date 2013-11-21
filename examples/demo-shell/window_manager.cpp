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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "window_manager.h"

#include "mir/shell/focus_controller.h"
#include "mir/shell/session.h"
#include "mir/shell/surface.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/compositor/compositor.h"

#include <linux/input.h>

#include <cassert>
#include <cstdlib>

namespace me = mir::examples;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mc = mir::compositor;

namespace
{
const int min_swipe_distance = 100;  // How long must a swipe be to act on?
}

me::WindowManager::WindowManager()
    : max_fingers(0)
{
}

void me::WindowManager::set_focus_controller(std::shared_ptr<msh::FocusController> const& controller)
{
    focus_controller = controller;
}

void me::WindowManager::set_display(std::shared_ptr<mg::Display> const& dpy)
{
    display = dpy;
}

void me::WindowManager::set_compositor(std::shared_ptr<mc::Compositor> const& cptor)
{
    compositor = cptor;
}

mir::geometry::Point average_pointer(MirMotionEvent const& motion)
{
    using namespace mir;
    using namespace geometry;

    int x = 0, y = 0, count = static_cast<int>(motion.pointer_count);

    for (int i = 0; i < count; i++)
    {
        x += motion.pointer_coordinates[i].x;
        y += motion.pointer_coordinates[i].y;
    }

    x /= count;
    y /= count;

    return Point{x, y};
}

bool me::WindowManager::handle(MirEvent const& event)
{
    // TODO: Fix android configuration and remove static hack ~racarr
    assert(focus_controller);
    assert(display);
    assert(compositor);

    if (event.type == mir_event_type_motion && focus_controller)
    {
        // FIXME: https://bugs.launchpad.net/mir/+bug/1197108
        MirMotionAction action = static_cast<MirMotionAction>(event.motion.action & ~0xff00);

        std::shared_ptr<msh::Session> app =
            focus_controller->focussed_application().lock();

        int fingers = static_cast<int>(event.motion.pointer_count);

        if (action == mir_motion_action_down || fingers > max_fingers)
            max_fingers = fingers;

        if (app)
        {
            if (max_fingers == 4 && action == mir_motion_action_up)
            {
                app->take_snapshot(
                    [&](mir::shell::Snapshot const& snapshot)
                    {
                        (void) snapshot;
                        printf("Snapshot taken\n");
                    });
            }
        }
    }
    return false;
}
