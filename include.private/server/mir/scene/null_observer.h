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

#ifndef MIR_SCENE_NULL_OBSERVER_H_
#define MIR_SCENE_NULL_OBSERVER_H_

#include "mir/scene/observer.h"

namespace mir
{
namespace scene
{
class NullObserver : public Observer
{
public:
    NullObserver() = default;
    virtual ~NullObserver() = default;

    void surface_added(Surface* surface);
    void surface_removed(Surface* surface);
    void surfaces_reordered();

    // Called at observer registration to notify of already existing surfaces.
    void surface_exists(Surface* surface);
    // Called when observer is unregistered, for example, to provide a place to
    // unregister SurfaceObservers which may have been added in surface_added/exists
    void end_observation();

protected:
    NullObserver(NullObserver const&) = delete;
    NullObserver& operator=(NullObserver const&) = delete;
};
}
} // namespace mir

#endif // MIR_SCENE_NULL_OBSERVER_H_
