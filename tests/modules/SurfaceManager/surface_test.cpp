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

#include <Unity/Application/application_manager.h>

#include <Unity/Application/application.h>
#include <Unity/Application/mirsurfaceitem.h>

#include "qtmir_test.h"
#include "stub_scene_surface.h"

using namespace qtmir;

namespace ms = mir::scene;
namespace mtd = mir::test::doubles;

class SurfaceTests : public ::testing::QtMirTest
{
public:
    SurfaceTests()
    {}

    QList<MirSurfaceItem*> listChildSurfaces(MirSurfaceItem* surface) {
        QList<MirSurfaceItem*> surfaces;
        surface->foreachChildSurface([&surfaces](MirSurfaceItem* child) { surfaces << child; });
        return surfaces;
    }
};

TEST_F(SurfaceTests, AddChildSurface)
{
    using namespace testing;

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msiParent = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    msiParent->addChildSurface(msiChild1);
    EXPECT_EQ(msiChild1->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1));

    msiParent->addChildSurface(msiChild2);
    EXPECT_EQ(msiChild2->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1, msiChild2));

    msiParent->addChildSurface(msiChild3);
    EXPECT_EQ(msiChild3->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1, msiChild2, msiChild3));
}

TEST_F(SurfaceTests, InsertChildSurface)
{
    using namespace testing;

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msiParent = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    msiParent->insertChildSurface(100, msiChild1); // test overflow
    EXPECT_EQ(msiChild1->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1));

    msiParent->insertChildSurface(0, msiChild2); // test insert before
    EXPECT_EQ(msiChild2->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild2, msiChild1));

    msiParent->insertChildSurface(1, msiChild3); // test before end
    EXPECT_EQ(msiChild3->parentSurface(), msiParent);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild2, msiChild3, msiChild1));
}

TEST_F(SurfaceTests, RemoveChildSurface)
{
    using namespace testing;

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msiParent = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    msiParent->addChildSurface(msiChild1);
    msiParent->addChildSurface(msiChild2);
    msiParent->addChildSurface(msiChild3);

    // remove surfaces
    msiParent->removeChildSurface(msiChild2);
    EXPECT_EQ(msiChild2->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1, msiChild3));

    msiParent->removeChildSurface(msiChild3);
    EXPECT_EQ(msiChild3->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1));

    msiParent->removeChildSurface(msiChild1);
    EXPECT_EQ(msiChild1->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), IsEmpty());
}

TEST_F(SurfaceTests, DeleleChildSurfaceRemovesFromParent)
{
    using namespace testing;

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msiParent = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    msiParent->addChildSurface(msiChild1);
    msiParent->addChildSurface(msiChild2);
    msiParent->addChildSurface(msiChild3);

    // remove surfaces
    delete msiChild2;
    EXPECT_EQ(msiChild2->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1, msiChild3));

    delete msiChild3;
    EXPECT_EQ(msiChild3->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), ElementsAre(msiChild1));

    delete msiChild1;
    EXPECT_EQ(msiChild1->parentSurface(), nullptr);
    EXPECT_THAT(listChildSurfaces(msiParent), IsEmpty());
}

TEST_F(SurfaceTests, DeleleParentDeletesChildren)
{
    using namespace testing;

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msiParent = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msiChild3 = new MirSurfaceItem(surface, nullptr);

    QList<QObject*> destroyed;
    QObject::connect(msiChild1, &QObject::destroyed, [&](QObject*) { destroyed << msiChild1; });
    QObject::connect(msiChild2, &QObject::destroyed, [&](QObject*) { destroyed << msiChild2; });
    QObject::connect(msiChild3, &QObject::destroyed, [&](QObject*) { destroyed << msiChild3; });

    // add surfaces
    msiParent->addChildSurface(msiChild1);
    msiParent->addChildSurface(msiChild2);
    msiParent->addChildSurface(msiChild3);

    // remove surfaces
    delete msiParent;
    EXPECT_THAT(destroyed, UnorderedElementsAre(msiChild1, msiChild2, msiChild3));
}
