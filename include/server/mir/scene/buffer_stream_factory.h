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

#ifndef MIR_SCENE_BUFFER_STREAM_FACTORY_H_
#define MIR_SCENE_BUFFER_STREAM_FACTORY_H_

#include "mir/frontend/buffer_stream_id.h"
#include <memory>

namespace mir
{
namespace compositor { class BufferStream; }
namespace graphics { struct BufferProperties; }
namespace frontend { class ClientBuffers; class BufferSink; }
namespace scene
{
class BufferStreamFactory
{
public:
    virtual ~BufferStreamFactory() = default;

    virtual std::shared_ptr<compositor::BufferStream> create_buffer_stream(
        frontend::BufferStreamId, std::shared_ptr<frontend::ClientBuffers> const& sink,
        int nbuffers, graphics::BufferProperties const& buffer_properties) = 0;
    virtual std::shared_ptr<compositor::BufferStream> create_buffer_stream(
        frontend::BufferStreamId, std::shared_ptr<frontend::ClientBuffers> const& sink,
        graphics::BufferProperties const& buffer_properties) = 0;
    virtual std::shared_ptr<frontend::ClientBuffers> create_buffer_map(
        std::shared_ptr<frontend::BufferSink> const& sink) = 0;

protected:
    BufferStreamFactory() = default;
    BufferStreamFactory(const BufferStreamFactory&) = delete;
    BufferStreamFactory& operator=(const BufferStreamFactory&) = delete;
};

}
}

#endif // MIR_SCENE_BUFFER_STREAM_FACTORY_H_
