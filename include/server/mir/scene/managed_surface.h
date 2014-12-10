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
#include <memory>

namespace mir { namespace scene {

class ManagedSurface : public SurfaceWrapper
{
public:
    ManagedSurface(std::shared_ptr<Surface> const&);
    virtual ~ManagedSurface();

    // TODO: Overrides for default window management policy
    bool visible() const override;
};

}} // namespace mir::scene

#endif // MIR_SCENE_MANAGED_SURFACE_H_
