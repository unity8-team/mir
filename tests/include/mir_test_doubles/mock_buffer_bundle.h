/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */
#ifndef MIR_TEST_DOUBLES_MOCK_BUFFER_BUNDLE_H_
#define MIR_TEST_DOUBLES_MOCK_BUFFER_BUNDLE_H_

#include "mir/compositor/buffer_handle.h"
#include "mir/compositor/buffer_bundle.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockBufferBundle : public compositor::BufferBundle
{
public:
    MockBufferBundle()
    {}
    ~MockBufferBundle() noexcept
    {}

    MOCK_METHOD1(client_acquire, void(std::function<void(graphics::Buffer*)>));
    MOCK_METHOD1(client_release, void(graphics::Buffer*));
    MOCK_CONST_METHOD0(properties, graphics::BufferProperties());
    MOCK_METHOD1(allow_framedropping, void(bool));
    MOCK_METHOD0(force_requests_to_complete, void());
    MOCK_METHOD1(resize, void(const geometry::Size &));
    MOCK_CONST_METHOD0(size, geometry::Size());
    MOCK_METHOD0(drop_old_buffers, void());
    MOCK_CONST_METHOD1(buffers_ready_for_compositor, int(void const*));
    compositor::BufferHandle compositor_acquire(void const*) override
        { return std::move(compositor::BufferHandle(nullptr, nullptr)); }
    compositor::BufferHandle snapshot_acquire() override
        { return std::move(compositor::BufferHandle(nullptr, nullptr)); }
    int buffers_free_for_client() const override { return 1; }
    void drop_client_requests() override {}
};

}
}
}

#endif /* MIR_TEST_DOUBLES_MOCK_BUFFER_BUNDLE_H_ */
