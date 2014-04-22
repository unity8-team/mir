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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_FAKE_RENDERABLE_H_
#define MIR_TEST_DOUBLES_FAKE_RENDERABLE_H_

#include "mir/graphics/renderable.h"
#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <gmock/gmock.h>
#include <atomic>

namespace mir
{
namespace test
{
namespace doubles
{

class FakeRenderable : public graphics::Renderable
{
public:
    FakeRenderable(int x, int y, int width, int height,
                   float opacity=1.0f,
                   bool rectangular=true,
                   bool visible=true,
                   bool posted=true)
        : rect{{x, y}, {width, height}},
          opacity(opacity),
          rectangular(rectangular),
          visible_(visible),
          posted(posted),
          duration_last_buffer_acquired{std::chrono::steady_clock::duration{0}}
    {
    }

    ID id() const override
    {
        return this;
    }

    float alpha() const override
    {
        return opacity;
    }

    glm::mat4 transformation() const override
    {
        return glm::mat4();
    }

    bool visible() const override
    {
        return visible_ && posted;
    }

    bool shaped() const override
    {
        return !rectangular;
    }

    void set_buffer(std::shared_ptr<graphics::Buffer> b)
    {
        buf = b;
    }

    std::shared_ptr<graphics::Buffer> buffer(void const*) const override
    {
        duration_last_buffer_acquired = std::chrono::steady_clock::now().time_since_epoch();
        return buf;
    }

    bool alpha_enabled() const override
    {
        return shaped() || alpha() < 1.0f;
    }

    geometry::Rectangle screen_position() const override
    {
        return rect;
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
    std::shared_ptr<graphics::Buffer> buf;
    mir::geometry::Rectangle rect;
    float opacity;
    bool rectangular;
    bool visible_;
    bool posted;
    mutable std::atomic<std::chrono::steady_clock::duration> duration_last_buffer_acquired;
};

} // namespace doubles
} // namespace test
} // namespace mir
#endif // MIR_TEST_DOUBLES_FAKE_RENDERABLE_H_
