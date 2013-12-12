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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir_toolkit/common.h"
#include "src/server/graphics/pixel_format_utils.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mg = mir::graphics;
TEST(MirPixelformat,contains_alpha)
{
    EXPECT_FALSE(mg::contains_alpha(mir_pixel_format_xbgr_8888));
    EXPECT_FALSE(mg::contains_alpha(mir_pixel_format_bgr_888));
    EXPECT_FALSE(mg::contains_alpha(mir_pixel_format_xrgb_8888));
    EXPECT_FALSE(mg::contains_alpha(mir_pixel_format_xbgr_8888));
    EXPECT_TRUE(mg::contains_alpha(mir_pixel_format_argb_8888));
    EXPECT_TRUE(mg::contains_alpha(mir_pixel_format_abgr_8888));
    EXPECT_FALSE(mg::contains_alpha(mir_pixel_format_invalid));
} 

TEST(MirPixelformat,red_channel_depths)
{
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_bgr_888));
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_xrgb_8888));
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_argb_8888));
    EXPECT_EQ(8, mg::red_channel_depth(mir_pixel_format_abgr_8888));
}

TEST(MirPixelformat,blue_channel_depths)
{
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_bgr_888));
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_xrgb_8888));
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_argb_8888));
    EXPECT_EQ(8, mg::blue_channel_depth(mir_pixel_format_abgr_8888));
}

TEST(MirPixelformat,green_channel_depths)
{
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_bgr_888));
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_xrgb_8888));
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_argb_8888));
    EXPECT_EQ(8, mg::green_channel_depth(mir_pixel_format_abgr_8888));
}


TEST(MirPixelformat,alpha_channel_depths)
{
    EXPECT_EQ(0, mg::alpha_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(0, mg::alpha_channel_depth(mir_pixel_format_bgr_888));
    EXPECT_EQ(0, mg::alpha_channel_depth(mir_pixel_format_xrgb_8888));
    EXPECT_EQ(0, mg::alpha_channel_depth(mir_pixel_format_xbgr_8888));
    EXPECT_EQ(8, mg::alpha_channel_depth(mir_pixel_format_argb_8888));
    EXPECT_EQ(8, mg::alpha_channel_depth(mir_pixel_format_abgr_8888));
}
