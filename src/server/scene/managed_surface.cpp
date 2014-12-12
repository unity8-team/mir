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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "managed_surface.h"

namespace mir { namespace scene {

ManagedSurface::ManagedSurface(std::shared_ptr<Surface> const& raw,
                       std::shared_ptr<shell::DisplayLayout> const& layout)
    : SurfaceWrapper(raw)
    , display_layout(layout)
    , restore_rect{raw->top_left(), raw->size()}
{
}

ManagedSurface::~ManagedSurface()
{
}

int ManagedSurface::configure(MirSurfaceAttrib attrib, int value)
{
    int new_value = value;

    if (attrib == mir_surface_attrib_state)
        new_value = set_state(static_cast<MirSurfaceState>(value));

    return SurfaceWrapper::configure(attrib, new_value);
}

MirSurfaceState ManagedSurface::set_state(MirSurfaceState desired)
{
    // TODO: Eventually this whole function should be atomic (LP: #1395957)
    geometry::Rectangle old_win{top_left(), size()}, new_win = old_win;

    auto fullscreen = old_win;
    display_layout->size_to_output(fullscreen);

    // TODO: Shell should define workarea to exclude panels/launchers/docks
    auto workarea = fullscreen;

    auto old_state = state();
    if (old_state != desired)
    {
        switch (old_state)
        {
        case mir_surface_state_minimized:
            show();
            break;
        case mir_surface_state_restored:
            restore_rect = old_win;
            break;
        default:
            break;
        }
    }

    switch (desired)
    {
    case mir_surface_state_fullscreen:
        new_win = fullscreen;
        break;
    case mir_surface_state_maximized:
        new_win = workarea;
        break;
    case mir_surface_state_vertmaximized:
        new_win.top_left.y = workarea.top_left.y;
        new_win.size.height = workarea.size.height;
        break;
    case mir_surface_state_restored:
        new_win = restore_rect;
        break;
    case mir_surface_state_minimized:
        hide();
        break;
    default:
        break;
    }

    if (new_win != old_win)
    {
        move_to(new_win.top_left);
        resize(new_win.size);
    }

    // TODO: In future the desired state may be rejected based on other
    //       factors such as surface type.
    return desired;
}

// TODO: More default window management policy here

}} // namespace mir::scene
