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

#include "mir/scene/managed_surface.h"

namespace mir { namespace scene {

ManagedSurface::ManagedSurface(std::shared_ptr<Surface> const& raw)
    : SurfaceWrapper(raw)
{
}

ManagedSurface::~ManagedSurface()
{
}

// A trivial example of window management policy:
bool ManagedSurface::visible() const
{
    return SurfaceWrapper::visible() &&
           SurfaceWrapper::state() != mir_surface_state_minimized;
}

// TODO: More default window management policy here

}} // namespace mir::scene
