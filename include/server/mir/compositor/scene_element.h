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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_COMPOSITOR_COMPOSITION_ELEMENT_H_
#define MIR_COMPOSITOR_COMPOSITION_ELEMENT_H_

#include "mir/graphics/renderable.h"

#include <memory>

namespace mir
{
namespace compositor
{

class SceneElement
{
public:
    virtual ~SceneElement() = default;

    virtual std::shared_ptr<graphics::Renderable> renderable() = 0;

protected:
    SceneElement() = default;
    SceneElement(SceneElement const&) = delete;
    SceneElement& operator=(SceneElement const&) = delete;
};

}
}

#endif // MIR_COMPOSITOR_COMPOSITION_ELEMENT_H_
