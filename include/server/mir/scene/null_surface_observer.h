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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SCENE_NULL_SURFACE_OBSERVER_H_
#define MIR_SCENE_NULL_SURFACE_OBSERVER_H_

#include "mir/scene/surface_observer.h"

namespace mir
{
namespace scene
{
class NullSurfaceObserver : public SurfaceObserver
{
public:
    NullSurfaceObserver() = default;
    virtual ~NullSurfaceObserver() = default;

    void surface_changed(Surface const&, Change) override;

protected:
    NullSurfaceObserver(NullSurfaceObserver const&) = delete;
    NullSurfaceObserver& operator=(NullSurfaceObserver const&) = delete;
};
}
}

#endif // MIR_SCENE_NULL_SURFACE_OBSERVER_H_
