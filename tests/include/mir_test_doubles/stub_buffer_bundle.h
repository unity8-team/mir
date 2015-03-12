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

#ifndef MIR_TEST_DOUBLES_NULL_BUFFER_BUNDLE_H_
#define MIR_TEST_DOUBLES_NULL_BUFFER_BUNDLE_H_

#include "mir/compositor/buffer_handle.h"
#include <mir/compositor/buffer_bundle.h>
#include <mir_test_doubles/stub_buffer.h>

namespace mir
{
namespace test
{
namespace doubles
{

class StubBufferBundle : public compositor::BufferBundle
{
public:
    StubBufferBundle()
    {
        stub_compositor_buffer = std::make_shared<StubBuffer>();
    }

    void client_acquire(
        std::function<void(graphics::Buffer* buffer)> complete) override
    {
        complete(&stub_client_buffer);
    }

    void client_release(graphics::Buffer*) override
    {
        ++nready;
    }

    compositor::BufferHandle compositor_acquire(void const*) override
    {
        --nready;
        return std::move(compositor::BufferHandle(stub_compositor_buffer, nullptr));
    }

    compositor::BufferHandle snapshot_acquire() override
    {
        return std::move(compositor::BufferHandle(stub_compositor_buffer, nullptr));
    }

    geometry::Size size() const override
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

    int buffers_free_for_client() const override { return 0; }

    int buffers_ready_for_compositor(void const*) const override { return nready; }

    void drop_old_buffers() override {}
    void drop_client_requests() override {}

    graphics::BufferProperties properties() const override
    { return graphics::BufferProperties(geometry::Size{0, 0},
        	                            mir_pixel_format_invalid,
        	                            graphics::BufferUsage::undefined); }

    StubBuffer stub_client_buffer;
    std::shared_ptr<graphics::Buffer> stub_compositor_buffer;
    int nready = 0;
};

}
}
} // namespace mir

#endif /* MIR_TEST_DOUBLES_NULL_BUFFER_BUNDLE_H_ */
