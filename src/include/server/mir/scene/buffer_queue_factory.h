/*
 * Copyright © 2012 Canonical Ltd.
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

#ifndef MIR_SCENE_BUFFER_QUEUE_FACTORY_H_
#define MIR_SCENE_BUFFER_QUEUE_FACTORY_H_

#include <memory>

namespace mir
{
namespace compositor { class BufferBundle; }
namespace graphics { struct BufferProperties; }

namespace scene
{
class BufferQueueFactory
{
public:
    virtual ~BufferQueueFactory() = default;

    virtual std::shared_ptr<compositor::BufferBundle> create_buffer_queue(
            int nbuffers, graphics::BufferProperties const& buffer_properties) = 0;

protected:
    BufferQueueFactory() = default;
    BufferQueueFactory(const BufferQueueFactory&) = delete;
    BufferQueueFactory& operator=(const BufferQueueFactory&) = delete;
};

}
}

#endif // MIR_SCENE_BUFFER_QUEUE_FACTORY_H_
