/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Unity/Application/application.h>
#include <Unity/Application/mirsurfaceitemmodel.h>
#include <Unity/Application/mirsurfaceitem.h>

#include "stub_scene_surface.h"

#include <gtest/gtest.h>

using namespace qtmir;

namespace ms = mir::scene;
namespace mtd = mir::test::doubles;

class SurfaceItemModelTests : public ::testing::Test
{
public:
    SurfaceItemModelTests()
    : mirSurface(std::make_shared<mtd::StubSceneSurface>(__LINE__))
    {}


    std::shared_ptr<ms::Surface> mirSurface;
};

TEST_F(SurfaceItemModelTests, TestInsert)
{
    using namespace testing;

    MirSurfaceItemModel model;
    MirSurfaceItem surface1(mirSurface, nullptr);
    MirSurfaceItem surface2(mirSurface, nullptr);
    MirSurfaceItem surface3(mirSurface, nullptr);
    MirSurfaceItem surface4(mirSurface, nullptr);
    MirSurfaceItem surface5(mirSurface, nullptr);

    // first
    model.insertSurface(0, &surface1);
    EXPECT_THAT(model.list(), ElementsAre(&surface1));

    // append
    model.insertSurface(model.count(), &surface2);
    EXPECT_THAT(model.list(), ElementsAre(&surface1, &surface2));

    // prepend
    model.insertSurface(0, &surface3);
    EXPECT_THAT(model.list(), ElementsAre(&surface3, &surface1, &surface2));

    // overflow
    model.insertSurface(14, &surface4);
    EXPECT_THAT(model.list(), ElementsAre(&surface3, &surface1, &surface2, &surface4));

    // middle
    model.insertSurface(2, &surface5);
    EXPECT_THAT(model.list(), ElementsAre(&surface3, &surface1, &surface5, &surface2, &surface4));
}

TEST_F(SurfaceItemModelTests, TestMove)
{
    using namespace testing;

    MirSurfaceItemModel model;
    MirSurfaceItem surface1(mirSurface, nullptr);
    MirSurfaceItem surface2(mirSurface, nullptr);
    MirSurfaceItem surface3(mirSurface, nullptr);
    MirSurfaceItem surface4(mirSurface, nullptr);
    MirSurfaceItem surface5(mirSurface, nullptr);

    model.insertSurface(0, &surface1);
    model.insertSurface(1, &surface2);
    model.insertSurface(2, &surface3);
    model.insertSurface(3, &surface4);
    model.insertSurface(4, &surface5);

    // move before
    model.insertSurface(0, &surface3);
    EXPECT_THAT(model.list(), ElementsAre(&surface3, &surface1, &surface2, &surface4, &surface5));

    // move after
    model.insertSurface(3, &surface1);
    EXPECT_THAT(model.list(), ElementsAre(&surface3, &surface2, &surface4, &surface1, &surface5));

    // move end
    model.insertSurface(4, &surface3);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface4, &surface1, &surface5, &surface3));

    // no move
    model.insertSurface(1, &surface4);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface4, &surface1, &surface5, &surface3));

    // move past end.
    model.insertSurface(model.count(), &surface2);
    EXPECT_THAT(model.list(), ElementsAre(&surface4, &surface1, &surface5, &surface3, &surface2));
}

TEST_F(SurfaceItemModelTests, TestRemove)
{
    using namespace testing;

    MirSurfaceItemModel model;
    MirSurfaceItem surface1(mirSurface, nullptr);
    MirSurfaceItem surface2(mirSurface, nullptr);
    MirSurfaceItem surface3(mirSurface, nullptr);
    MirSurfaceItem surface4(mirSurface, nullptr);
    MirSurfaceItem surface5(mirSurface, nullptr);
    MirSurfaceItem surface6(mirSurface, nullptr);

    model.insertSurface(0, &surface1);
    model.insertSurface(1, &surface2);
    model.insertSurface(2, &surface3);
    model.insertSurface(3, &surface4);
    model.insertSurface(4, &surface5);

    // first
    model.removeSurface(&surface1);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface3, &surface4, &surface5));

    // just-removed
    model.removeSurface(&surface1);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface3, &surface4, &surface5));

    // last
    model.removeSurface(&surface5);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface3, &surface4));

    // middle
    model.removeSurface(&surface3);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface4));

    // non-existant
    model.removeSurface(&surface6);
    EXPECT_THAT(model.list(), ElementsAre(&surface2, &surface4));
}
