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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_RENDERABLE_H_
#define MIR_TEST_DOUBLES_STUB_RENDERABLE_H_

#include "mir_test_doubles/stub_buffer.h"
#include <mir/graphics/renderable.h>
#include <memory>
#include <atomic>

namespace mir
{
namespace test
{
namespace doubles
{

class StubRenderable : public graphics::Renderable
{
public:
    StubRenderable() :
        duration_last_buffer_acquired{std::chrono::steady_clock::duration{0}}
    {
    }

    ID id() const override
    {
        return this;
    }
    std::shared_ptr<graphics::Buffer> buffer(void const*) const override
    {
        duration_last_buffer_acquired = std::chrono::steady_clock::now().time_since_epoch();
        return std::make_shared<StubBuffer>();
    }
    bool alpha_enabled() const
    {
        return false;
    }
    geometry::Rectangle screen_position() const
    {
        return {{},{}};
    }
    float alpha() const override
    {
        return 1.0f;
    }
    glm::mat4 transformation() const override
    {
        return trans;
    }
    bool visible() const
    {
        return true;
    }
    bool shaped() const override
    {
        return false;
    }

    int buffers_ready_for_compositor() const override
    {
        return 1;
    }

    std::chrono::steady_clock::time_point time_last_buffer_acquired() const override
    {
        return std::chrono::steady_clock::time_point{duration_last_buffer_acquired};
    }

private:
    glm::mat4 trans;
    mutable std::atomic<std::chrono::steady_clock::duration> duration_last_buffer_acquired;
};

}
}
}

#endif /* MIR_TEST_DOUBLES_STUB_RENDERABLE_H_ */
