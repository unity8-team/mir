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

#ifndef MIR_SCENE_MANAGED_SURFACE_H_
#define MIR_SCENE_MANAGED_SURFACE_H_

#include "mir/scene/surface_wrapper.h"
#include "mir/shell/display_layout.h"
#include "mir/geometry/rectangle.h"
#include "mir_toolkit/common.h"
#include <memory>

namespace mir { namespace scene {

class ManagedSurface : public SurfaceWrapper
{
public:
    ManagedSurface(std::shared_ptr<Surface> const&,
                   std::shared_ptr<shell::DisplayLayout> const&);
    virtual ~ManagedSurface();

    int configure(MirSurfaceAttrib attrib, int value) override;

    // TODO: More overrides for default window management policy

private:
    std::shared_ptr<shell::DisplayLayout> const display_layout;
    geometry::Rectangle restore_rect;

    MirSurfaceState set_state(MirSurfaceState state);
};

}} // namespace mir::scene

#endif // MIR_SCENE_MANAGED_SURFACE_H_
