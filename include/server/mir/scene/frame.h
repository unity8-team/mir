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
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_SCENE_FRAME_H_
#define MIR_SCENE_FRAME_H_

#include "mir/scene/surface.h"
#include "mir/geometry/length.h"

namespace mir
{
namespace scene
{

class Surface;

class Frame
{
public:
    struct Extents
    {
        geometry::Length top, bottom, left, right;
    };

    Frame(Surface& client);
    virtual ~Frame() = default;

    virtual Extents extents() const = 0;

    //virtual std::shared_ptr<Surface> heading() = 0; // titlebar/menu/tabs

protected:
    Surface& client;
};

class NullFrame : public Frame
{
public:
    NullFrame(Surface& client);
    Extents extents() const override;
};

} // namespace scene
} // namespace mir

#endif // MIR_SCENE_FRAME_H_
