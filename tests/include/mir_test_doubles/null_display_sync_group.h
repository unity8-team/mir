/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_NULL_DISPLAY_SYNC_GROUP_H_
#define MIR_TEST_DOUBLES_NULL_DISPLAY_SYNC_GROUP_H_

#include "mir/graphics/display.h"
#include "mir/geometry/size.h"
#include "null_display_buffer.h"
#include "stub_display_buffer.h"
#include <thread>
#include <chrono>

namespace mir
{
namespace test
{
namespace doubles
{

struct StubDisplaySyncGroup : graphics::DisplaySyncGroup
{
public:
    StubDisplaySyncGroup(std::vector<geometry::Rectangle> const& output_rects)
        : StubDisplaySyncGroup(output_rects, std::chrono::milliseconds{0})
    {
    }

    StubDisplaySyncGroup(geometry::Size sz)
        : StubDisplaySyncGroup({{{0,0}, sz}}, std::chrono::milliseconds{0})
    {
    }

    StubDisplaySyncGroup(std::vector<geometry::Rectangle> const& output_rects,
                         std::chrono::milliseconds vsync_interval)
        : output_rects{output_rects},
          vsync_interval{vsync_interval}
    {
        for (auto const& output_rect : output_rects)
            display_buffers.emplace_back(output_rect);
    }

    void for_each_display_buffer(std::function<void(graphics::DisplayBuffer&)> const& f) override
    {
        for (auto& db : display_buffers)
            f(db);
    }

    void post() override
    {
        /* yield() is needed to ensure reasonable runtime under valgrind for some tests */
        if (vsync_interval == vsync_interval.zero())
            std::this_thread::yield();
        else
            std::this_thread::sleep_for(vsync_interval);
    }

private:
    std::vector<geometry::Rectangle> const output_rects;
    std::vector<StubDisplayBuffer> display_buffers;
    std::chrono::milliseconds vsync_interval{0};
};

struct NullDisplaySyncGroup : graphics::DisplaySyncGroup
{
    void for_each_display_buffer(std::function<void(graphics::DisplayBuffer&)> const& f) override
    {
        f(db);
    }
    virtual void post() override
    {
        /* yield() is needed to ensure reasonable runtime under valgrind for some tests */
        std::this_thread::yield();
    }
    NullDisplayBuffer db;
};

}
}
}

#endif /* MIR_TEST_DOUBLES_NULL_DISPLAY_SYNC_GROUP_H_ */
