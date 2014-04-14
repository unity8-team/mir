/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_COMPOSITOR_SCENE_H_
#define MIR_COMPOSITOR_SCENE_H_

#include "mir/geometry/forward.h"
#include "mir/graphics/renderable.h"

#include <memory>
#include <functional>

namespace mir
{
namespace compositor
{

class Scene
{
public:
    virtual ~Scene() {}

    /**
     * Generate a valid list of renderables based on the current state of the Scene.
     * \returns a list of mg::Renderables. The list is in stacking order from back to front.
     */
    virtual graphics::RenderableList generate_renderable_list() const = 0;

    /**
     * Sets a callback to be called whenever the state of the
     * Scene changes.
     *
     * The supplied callback should not directly or indirectly (e.g.,
     * by changing a property of a surface) change the state of
     * the Scene, otherwise a deadlock may occur.
     */
    virtual void set_change_callback(std::function<void()> const& f) = 0;

protected:
    Scene() = default;

private:
    Scene(Scene const&) = delete;
    Scene& operator=(Scene const&) = delete;
};

}
}

#endif /* MIR_COMPOSITOR_SCENE_H_ */
