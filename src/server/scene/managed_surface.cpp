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
    : SurfaceWrapper(raw), display_layout(layout)
{
}

ManagedSurface::~ManagedSurface()
{
}

int ManagedSurface::configure(MirSurfaceAttrib attrib, int value)
{
    int new_value = SurfaceWrapper::configure(attrib, value);

    if (attrib == mir_surface_attrib_state)
        set_state(static_cast<MirSurfaceState>(new_value));

    return new_value;
}

void ManagedSurface::set_state(MirSurfaceState desired)
{
    if (desired == mir_surface_state_minimized ||
        desired == mir_surface_state_restored)
        return;

    // TODO: Make all this an atomic operation (LP: #1395957)

    geometry::Rectangle new_win, old_win{top_left(), size()};

    auto fullscreen = old_win;
    display_layout->size_to_output(fullscreen);

    // TODO: Limit workarea to exclude panels/launchers defined by the shell
    auto workarea = fullscreen;

    switch (desired)
    {
    case mir_surface_state_fullscreen:
        new_win = fullscreen;
        break;
    case mir_surface_state_maximized:
        new_win = workarea;
        break;
    case mir_surface_state_vertmaximized:
        new_win.top_left.x = old_win.top_left.x;
        new_win.top_left.y = workarea.top_left.y;
        new_win.size.width = old_win.size.width;
        new_win.size.height = workarea.size.height;
        break;
    default:
        new_win = old_win;
        break;
    }

    if (old_win != new_win)
    {
        resize(new_win.size);
        move_to(new_win.top_left);
    }
}

// TODO: More default window management policy here

}} // namespace mir::scene
