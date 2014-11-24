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

#include "src/server/shell/graphics_display_layout.h"

#include "mir_test_doubles/stub_display.h"

#include <vector>
#include <tuple>

#include <gtest/gtest.h>

namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;

namespace
{

struct StubDisplay : mtd::StubDisplay
{
    StubDisplay()
        : mtd::StubDisplay{
              std::vector<geom::Rectangle>{
                  {{0,0}, {800,600}},
                  {{0,600}, {100,100}},
                  {{800,0}, {100,100}}
              }
          }
    {
    }
};

}

TEST(GraphicsDisplayLayoutTest, ClipsCorrectly)
{
    auto stub_display = std::make_shared<StubDisplay>();

    msh::GraphicsDisplayLayout display_layout{stub_display};

    std::vector<std::tuple<geom::Rectangle,geom::Rectangle>> rect_tuples{
        std::make_tuple(
            geom::Rectangle{geom::Point{0,0}, geom::Size{800,600}},
            geom::Rectangle{geom::Point{0,0}, geom::Size{800,600}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{750,50}, geom::Size{100,100}},
            geom::Rectangle{geom::Point{750,50}, geom::Size{50,100}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{899,99}, geom::Size{100,100}},
            geom::Rectangle{geom::Point{899,99}, geom::Size{1,1}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{-1,-1}, geom::Size{100,100}},
            geom::Rectangle{geom::Point{-1,-1}, geom::Size{0,0}})
    };

    for (auto const& t : rect_tuples)
    {
        auto clipped_rect = std::get<0>(t);
        auto const expected_rect = std::get<1>(t);
        display_layout.clip_to_output(clipped_rect);
        EXPECT_EQ(expected_rect, clipped_rect);
    }
}

TEST(GraphicsDisplayLayoutTest, MakesFullscreenInCorrectScreen)
{
    auto stub_display = std::make_shared<StubDisplay>();

    msh::GraphicsDisplayLayout display_layout{stub_display};

    std::vector<std::tuple<geom::Rectangle,geom::Rectangle>> rect_tuples{
        std::make_tuple(
            geom::Rectangle{geom::Point{0,0}, geom::Size{800,600}},
            geom::Rectangle{geom::Point{0,0}, geom::Size{800,600}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{750,50}, geom::Size{150,130}},
            geom::Rectangle{geom::Point{0,0}, geom::Size{800,600}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{899,99}, geom::Size{30,40}},
            geom::Rectangle{geom::Point{800,0}, geom::Size{100,100}}),
        std::make_tuple(
            geom::Rectangle{geom::Point{-1,-1}, geom::Size{100,100}},
            geom::Rectangle{geom::Point{0,0}, geom::Size{0,0}})
    };

    for (auto const& t : rect_tuples)
    {
        auto fullscreen_rect = std::get<0>(t);
        auto const expected_rect = std::get<1>(t);
        display_layout.size_to_output(fullscreen_rect);
        EXPECT_EQ(expected_rect, fullscreen_rect);
    }
}

TEST(GraphicsDisplayLayoutTest, PlaceInOutputPlacesInCorrectOutput)
{
    auto stub_display = std::make_shared<StubDisplay>();

    msh::GraphicsDisplayLayout display_layout{stub_display};

    std::vector<std::tuple<mg::DisplayConfigurationOutputId,geom::Rectangle,geom::Rectangle>> rect_tuples
    {
        std::make_tuple(
            mg::DisplayConfigurationOutputId{1},
            geom::Rectangle{{0,0}, {800,600}},
            geom::Rectangle{{0,0}, {800,600}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{1},
            geom::Rectangle{{750,50}, {800,600}},
            geom::Rectangle{{0,0}, {800,600}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{2},
            geom::Rectangle{{899,99}, {100,100}},
            geom::Rectangle{{0,600}, {100,100}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{3},
            geom::Rectangle{{-1,-1}, {100,100}},
            geom::Rectangle{{800,0}, {100,100}})
    };

    for (auto const& t : rect_tuples)
    {
        auto const output_id = std::get<0>(t);
        auto submitted_rect = std::get<1>(t);
        auto const expected_rect = std::get<2>(t);
        display_layout.place_in_output(output_id, submitted_rect);
        EXPECT_EQ(expected_rect, submitted_rect);
    }
}

TEST(GraphicsDisplayLayoutTest, PlaceInOutputThrowsOnNonFullscreenRequest)
{
    auto stub_display = std::make_shared<StubDisplay>();

    msh::GraphicsDisplayLayout display_layout{stub_display};

    std::vector<std::tuple<mg::DisplayConfigurationOutputId,geom::Rectangle>> rect_tuples
    {
        std::make_tuple(
            mg::DisplayConfigurationOutputId{1},
            geom::Rectangle{{0,0}, {801,600}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{1},
            geom::Rectangle{{750,50}, {800,599}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{2},
            geom::Rectangle{{899,99}, {1,1}}),
        std::make_tuple(
            mg::DisplayConfigurationOutputId{3},
            geom::Rectangle{{-1,-1}, {0,0}}),
    };

    for (auto const& t : rect_tuples)
    {
        auto const output_id = std::get<0>(t);
        auto submitted_rect = std::get<1>(t);
        EXPECT_THROW({
            display_layout.place_in_output(output_id, submitted_rect);
        }, std::runtime_error);
    }
}
