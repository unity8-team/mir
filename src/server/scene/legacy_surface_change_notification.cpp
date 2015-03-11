/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "legacy_surface_change_notification.h"

namespace mir { namespace scene {

LegacySurfaceChangeNotification::LegacySurfaceChangeNotification(
    std::function<void()> const& notify_scene_change,
    std::function<void(int)> const& notify_buffer_change) :
    notify_scene_change(notify_scene_change),
    notify_buffer_change(notify_buffer_change)
{
}

void LegacySurfaceChangeNotification::surface_changed(
    Surface const&, Change change)
{
    switch (change)
    {
    case size:
    case position:
    case visibility:
    case opacity:
    case transformation:
    case input_mode:
        notify_scene_change();
        break;
    case content:
        notify_buffer_change(1); // TODO remove parameter
        break;
    default:
        break;
    }
}

} }  // namespace mir::scene
