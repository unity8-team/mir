/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_RENDERABLE_H_
#define MIR_GRAPHICS_RENDERABLE_H_

#include <mir/geometry/rectangle.h>
#include <glm/glm.hpp>
#include <memory>
#include <list>

namespace mir
{
namespace graphics
{

class Buffer;
class Renderable
{
public:
    /**
     * Return the next buffer that should be composited/rendered.
     *
     * \param [in] user_id An arbitrary unique identifier used to distinguish
     *                     separate threads/monitors/components which need
     *                     to concurrently receive the same buffer. Calling
     *                     with the same user_id will return a new (different)
     *                     buffer to that user each time. For consistency,
     *                     all callers need to determine their user_id in the
     *                     same way (e.g. always use "this" pointer).
     */
    virtual std::shared_ptr<Buffer> buffer(void const* user_id) const = 0;

    virtual bool alpha_enabled() const = 0;
    virtual geometry::Rectangle screen_position() const = 0;

    // These are from the old CompositingCriteria. There is a little bit
    // of function overlap with the above functions still.
    virtual float alpha() const = 0;

    /**
     * Transformation returns the transformation matrix that should be applied
     * to the surface. By default when there are no transformations this will
     * be the identity matrix.
     *
     * \warning As this functionality is presently only used by
     *          mir_demo_standalone_render_surfaces for rotations it may be
     *          deprecated in future. It is expected that real transformations
     *          may become more transient things (e.g. applied by animation
     *          logic externally instead of being a semi-permanent attribute of
     *          the surface itself).
     */
    virtual glm::mat4 transformation() const = 0;

    /**
     * TODO: Its a bit questionable that we have this member function, why not 
     *       just trim the renderable from the RenderableList? Its convenient
     *       to have this function temporarily while refactoring --kdub
     */ 
    virtual bool visible() const = 0;

    virtual bool shaped() const = 0;  // meaning the pixel format has alpha
    virtual int buffers_ready_for_compositor() const = 0;

protected:
    Renderable() = default;
    virtual ~Renderable() = default;
    Renderable(Renderable const&) = delete;
    Renderable& operator=(Renderable const&) = delete;
};

// XXX Would performance be better with a vector?
typedef std::list<std::shared_ptr<Renderable>> RenderableList;

}
}

#endif /* MIR_GRAPHICS_RENDERABLE_H_ */
