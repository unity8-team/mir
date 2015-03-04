/*
 * Copyright © 2012, 2015 Canonical Ltd.
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
 * Authored by:
 *  Alan Griffiths <alan@octopull.co.uk>
 *  Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_COMPOSITOR_BUFFER_QUEUE_FACTORY_H_
#define MIR_COMPOSITOR_BUFFER_QUEUE_FACTORY_H_

#include "mir/scene/buffer_queue_factory.h"
#include "mir/compositor/frame_dropping_policy_factory.h"

#include <memory>

namespace mir
{
namespace graphics
{
class GraphicBufferAllocator;
}
namespace compositor
{

class BufferQueueFactory : public scene::BufferQueueFactory
{
public:
	BufferQueueFactory(std::shared_ptr<graphics::GraphicBufferAllocator> const& gralloc,
                        std::shared_ptr<FrameDroppingPolicyFactory> const& policy_factory);

    virtual ~BufferQueueFactory() {}

    virtual std::shared_ptr<BufferBundle> create_buffer_queue(
        graphics::BufferProperties const& buffer_properties);

private:
    std::shared_ptr<graphics::GraphicBufferAllocator> gralloc;
    std::shared_ptr<FrameDroppingPolicyFactory> const policy_factory;
};

}
}


#endif /* MIR_COMPOSITOR_BUFFER_QUEUE_FACTORY_H_ */
