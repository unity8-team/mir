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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_NULL_BUFFER_STREAM_H_
#define MIR_TEST_DOUBLES_NULL_BUFFER_STREAM_H_

#include <mir/compositor/buffer_stream.h>
#include <mir/test/doubles/stub_buffer.h>
#include "mir/test/current_thread_name.h"

namespace mir
{
namespace test
{
namespace doubles
{

class StubBufferStream : public compositor::BufferStream
{
public:
    StubBufferStream()
    {
        stub_compositor_buffer = std::make_shared<StubBuffer>();
    }


    std::shared_ptr<graphics::Buffer> lock_compositor_buffer(void const*) override
    {
        --nready;
        return stub_compositor_buffer;
    }

    geometry::Size stream_size() override
    {
        return geometry::Size();
    }

    void resize(geometry::Size const&) override
    {
    }

    void force_requests_to_complete() override
    {
    }

    void allow_framedropping(bool) override
    {
    }

    int buffers_ready_for_compositor(void const*) const override { return nready; }

    void drop_old_buffers() override {}
    void swap_buffers(graphics::Buffer* b, std::function<void(graphics::Buffer*)> complete) override
    {
        if (b) ++nready;
        complete(&stub_client_buffer);
    }
    void with_most_recent_buffer_do(std::function<void(graphics::Buffer&)> const& fn)
    {
        thread_name = current_thread_name();
        fn(*stub_compositor_buffer);
    }
    MirPixelFormat pixel_format() const { return mir_pixel_format_abgr_8888; }
    void add_observer(std::shared_ptr<scene::SurfaceObserver> const&) {}
    void remove_observer(std::weak_ptr<scene::SurfaceObserver> const&) {}
    bool has_submitted_buffer() const { return true; }
    graphics::BufferID allocate_buffer(graphics::BufferProperties const&) { return graphics::BufferID{}; }
    void remove_buffer(graphics::BufferID) {}
    void with_buffer(graphics::BufferID, std::function<void(graphics::Buffer&)> const&) {}
    void set_scale(float) {}

    StubBuffer stub_client_buffer;
    std::shared_ptr<graphics::Buffer> stub_compositor_buffer;
    int nready = 0;
    std::string thread_name;
};

}
}
} // namespace mir

#endif /* MIR_TEST_DOUBLES_NULL_BUFFER_STREAM_H_ */
