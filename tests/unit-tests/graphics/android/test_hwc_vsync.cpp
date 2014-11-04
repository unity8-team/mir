/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "src/platform/graphics/android/hwc_vsync.h"

#include "mir/graphics/display_configuration.h"

#include <stdexcept>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mga = mg::android;

namespace
{
struct HWCVsyncTest : public testing::Test
{
    mga::HWCVsync syncer;

    mg::DisplayConfigurationOutputId const output_id{0};
    mg::DisplayConfigurationOutputId const invalid_output_id{1};
};
}

TEST_F(HWCVsyncTest, initially_last_vsync_is_zero)
{
    EXPECT_EQ(std::chrono::nanoseconds::zero(), syncer.last_vsync_for(output_id));
}

// No multimonitor support on android drivers yet so non zero output ID is invalid.
TEST_F(HWCVsyncTest, throws_exception_on_non_zero_output_id)
{
    EXPECT_THROW({
            syncer.last_vsync_for(invalid_output_id);
    }, std::runtime_error);
}

TEST_F(HWCVsyncTest, takes_time_from_last_vsync_notification)
{
    std::chrono::nanoseconds first_time{1}, second_time{2};
    
    EXPECT_EQ(std::chrono::nanoseconds::zero(), syncer.last_vsync_for(output_id));

    syncer.notify_vsync(first_time);
    EXPECT_EQ(first_time, syncer.last_vsync_for(output_id));
    EXPECT_EQ(first_time, syncer.last_vsync_for(output_id));

    syncer.notify_vsync(second_time);
    EXPECT_EQ(second_time, syncer.last_vsync_for(output_id));
    EXPECT_EQ(second_time, syncer.last_vsync_for(output_id));
}
