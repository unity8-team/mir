/*
 * Copyright © 2012 Canonical Ltd.
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

#include "mir_test_doubles/mock_viewable_area.h"
#include "mir_test/fake_shared.h"

#include "src/server/input/android/android_input_reader_policy.h"
#include "mir/geometry/rectangle.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

static geom::Rectangle const default_view_area = geom::Rectangle{geom::Point(),
                                                                 geom::Size{geom::Width(1600),
                                                                            geom::Height(1400)}};

namespace
{
using namespace::testing;

class AndroidInputReaderPolicySetup : public testing::Test
{
public:
    void SetUp()
    {
        reader_policy = std::make_shared<mia::InputReaderPolicy>(
            mt::fake_shared(viewable_area),
            std::shared_ptr<mi::CursorListener>());

        ON_CALL(viewable_area, view_area()).WillByDefault(Return(default_view_area));
    }
    mtd::MockViewableArea viewable_area;
    std::shared_ptr<mia::InputReaderPolicy> reader_policy;
};
}

TEST_F(AndroidInputReaderPolicySetup, configuration_has_display_info_filled_from_view_area)
{
    static int32_t const testing_display_id = 0;
    static bool const testing_display_is_external = false;

    EXPECT_CALL(viewable_area, view_area()).Times(1);

    droidinput::InputReaderConfiguration configuration;
    reader_policy->getReaderConfiguration(&configuration);

    int32_t configured_height, configured_width, configured_orientation;

    bool configuration_has_display_info = configuration.getDisplayInfo(
        testing_display_id, testing_display_is_external, &configured_width, &configured_height,
        &configured_orientation);

    ASSERT_TRUE(configuration_has_display_info);

    EXPECT_EQ(default_view_area.size.width.as_uint32_t(),  (uint32_t)configured_width);
    EXPECT_EQ(default_view_area.size.height.as_uint32_t(), (uint32_t)configured_height);
}
