/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Robert Carr <robert.carr@canoniacl.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_BUFFER_STREAM_FACTORY_H_
#define MIR_TEST_DOUBLES_STUB_BUFFER_STREAM_FACTORY_H_

#include "mir/frontend/client_buffers.h"
#include "mir/scene/buffer_stream_factory.h"
#include "stub_buffer_stream.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubClientBuffers : frontend::ClientBuffers
{
    graphics::BufferID add_buffer(std::shared_ptr<graphics::Buffer> const&) override
    {
        return {};
    }
    void remove_buffer(graphics::BufferID) override
    {
    }
    std::shared_ptr<graphics::Buffer> get(graphics::BufferID) const override
    {
        return buffer;
    }
    void send_buffer(graphics::BufferID) override
    {
    }
    void receive_buffer(graphics::BufferID) override
    {
    }
    std::shared_ptr<graphics::Buffer> buffer;
};

struct StubBufferStreamFactory : public scene::BufferStreamFactory
{
    std::shared_ptr<compositor::BufferStream> create_buffer_stream(
        frontend::BufferStreamId i, std::shared_ptr<frontend::ClientBuffers> const& s,
        int, graphics::BufferProperties const& p) { return create_buffer_stream(i, s, p); }
    std::shared_ptr<compositor::BufferStream> create_buffer_stream(
        frontend::BufferStreamId, std::shared_ptr<frontend::ClientBuffers> const&,
        graphics::BufferProperties const&) { return std::make_shared<StubBufferStream>(); }
    std::shared_ptr<frontend::ClientBuffers> create_buffer_map(
        std::shared_ptr<frontend::BufferSink> const&)
    {
        return std::make_shared<StubClientBuffers>();
    }
};
}
}
}

#endif // MIR_TEST_DOUBLES_STUB_BUFFER_STREAM_FACTORY_H_
