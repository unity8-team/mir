/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/input/display_input_region.h"

#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/stub_display.h"
#include "mir_test_doubles/stub_display_changer.h"

#include <vector>
#include <tuple>

#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mg = mir::graphics;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;

namespace
{
std::vector<geom::Rectangle> const rects{
    geom::Rectangle{{0,0}, {800,600}},
    geom::Rectangle{{0,600}, {100,100}},
    geom::Rectangle{{800,0}, {100,100}}
};
}

TEST(DisplayInputRegionTest, returns_correct_bounding_rectangle)
{
    geom::Rectangle const expected_bounding_rect{geom::Point{0,0}, geom::Size{100,100}};
    auto stub_display = std::make_shared<mtd::StubDisplay>(rects);
    auto stub_display_changer = std::make_shared<mtd::StubDisplayChanger>();

    mi::DisplayInputRegion input_region{*stub_display->configuration(), stub_display_changer};

    auto rect = input_region.bounding_rectangle();
    EXPECT_EQ(expected_bounding_rect, rect);
}

TEST(DisplayInputRegionTest, confines_point_to_closest_valid_position)
{
    auto stub_display = std::make_shared<mtd::StubDisplay>(rects);
    auto stub_display_changer = std::make_shared<mtd::StubDisplayChanger>();

    mi::DisplayInputRegion input_region{*stub_display->configuration(), stub_display_changer};

    std::vector<std::tuple<geom::Point,geom::Point>> point_tuples{
        std::make_tuple(geom::Point{0,0}, geom::Point{0,0}),
        std::make_tuple(geom::Point{900,50}, geom::Point{99,50}),
        std::make_tuple(geom::Point{850,100}, geom::Point{99,99}),
        std::make_tuple(geom::Point{801,100}, geom::Point{99,99}),
        std::make_tuple(geom::Point{800,101}, geom::Point{99,99}),
        std::make_tuple(geom::Point{800,600}, geom::Point{99,99}),
        std::make_tuple(geom::Point{-1,700}, geom::Point{0,99}),
        std::make_tuple(geom::Point{-1,-1}, geom::Point{0,0}),
        std::make_tuple(geom::Point{-1,50}, geom::Point{0,50}),
        std::make_tuple(geom::Point{799,-1}, geom::Point{99,0}),
        std::make_tuple(geom::Point{800,-1}, geom::Point{99,0})
    };

    for (auto const& t : point_tuples)
    {
        geom::Point confined_point{std::get<0>(t)};
        geom::Point const expected_point{std::get<1>(t)};
        input_region.confine(confined_point);
        EXPECT_EQ(expected_point, confined_point);
    }

}
