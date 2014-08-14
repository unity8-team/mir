/*
 * Copyright (C) 2013 Canonical, Ltd.
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

class ApplicationTests : public ::testing::QtMirTest
{
public:
    ApplicationTests()
    {}

    QList<MirSurfaceItem*> listPromptSurfaces(Application* app) {
        QList<MirSurfaceItem*> surfaces;
        app->foreachPromptSurface([&surfaces](MirSurfaceItem* surface) { surfaces << surface; });
        return surfaces;
    }

};

TEST_F(ApplicationTests, AddPromptSurface)
{
    using namespace testing;

    QString appId("test-app");

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSharedPointer<Application> app(new Application(taskController,
                                                    desktopFileReaderFactory.createInstance(appId, QFileInfo()),
                                                    Application::Running,
                                                    QStringList(),
                                                    mirConfig->the_prompt_session_manager(),
                                                    &applicationManager));

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msi1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    app->addPromptSurface(msi1);
    EXPECT_EQ(msi1->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1));

    app->addPromptSurface(msi2);
    EXPECT_EQ(msi2->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1, msi2));

    app->addPromptSurface(msi3);
    EXPECT_EQ(msi3->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1, msi2, msi3));
}

TEST_F(ApplicationTests, InsertPromptSurface)
{
    using namespace testing;

    QString appId("test-app");

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSharedPointer<Application> app(new Application(taskController,
                                                    desktopFileReaderFactory.createInstance(appId, QFileInfo()),
                                                    Application::Running,
                                                    QStringList(),
                                                    mirConfig->the_prompt_session_manager(),
                                                    &applicationManager));

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msi1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    app->insertPromptSurface(100, msi1); // test overflow
    EXPECT_EQ(msi1->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1));

    app->insertPromptSurface(0, msi2); // test insert before
    EXPECT_EQ(msi2->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi2, msi1));

    app->insertPromptSurface(1, msi3); // test before end
    EXPECT_EQ(msi3->application(), app);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi2, msi3, msi1));
}

TEST_F(ApplicationTests, RemovePromptSurface)
{
    using namespace testing;

    QString appId("test-app");

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSharedPointer<Application> app(new Application(taskController,
                                                    desktopFileReaderFactory.createInstance(appId, QFileInfo()),
                                                    Application::Running,
                                                    QStringList(),
                                                    mirConfig->the_prompt_session_manager(),
                                                    &applicationManager));

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msi1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    app->addPromptSurface(msi1);
    app->addPromptSurface(msi2);
    app->addPromptSurface(msi3);

    // remove surfaces
    app->removeSurface(msi2);
    EXPECT_EQ(msi2->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1, msi3));

    app->removeSurface(msi3);
    EXPECT_EQ(msi3->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1));

    app->removeSurface(msi1);
    EXPECT_EQ(msi1->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), IsEmpty());
}

TEST_F(ApplicationTests, DelelePromptSurfaceRemovesFromApplcation)
{
    using namespace testing;

    QString appId("test-app");

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSharedPointer<Application> app(new Application(taskController,
                                                    desktopFileReaderFactory.createInstance(appId, QFileInfo()),
                                                    Application::Running,
                                                    QStringList(),
                                                    mirConfig->the_prompt_session_manager(),
                                                    &applicationManager));

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msi1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    app->addPromptSurface(msi1);
    app->addPromptSurface(msi2);
    app->addPromptSurface(msi3);

    // delete surfaces
    delete msi2;
    EXPECT_EQ(msi2->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1, msi3));

    // delete surfaces
    delete msi3;
    EXPECT_EQ(msi3->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), ElementsAre(msi1));

    // delete surfaces
    delete msi1;
    EXPECT_EQ(msi1->application(), nullptr);
    EXPECT_THAT(listPromptSurfaces(app.data()), IsEmpty());
}

TEST_F(ApplicationTests, DeleleAppDeletesPromptSurfaces)
{
    using namespace testing;

    QString appId("test-app");

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSharedPointer<Application> app(new Application(taskController,
                                                    desktopFileReaderFactory.createInstance(appId, QFileInfo()),
                                                    Application::Running,
                                                    QStringList(),
                                                    mirConfig->the_prompt_session_manager(),
                                                    &applicationManager));

    auto surface = std::make_shared<mtd::StubSceneSurface>(__LINE__);
    MirSurfaceItem* msi1 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi2 = new MirSurfaceItem(surface, nullptr);
    MirSurfaceItem* msi3 = new MirSurfaceItem(surface, nullptr);

    // add surfaces
    app->addPromptSurface(msi1);
    app->addPromptSurface(msi2);
    app->addPromptSurface(msi3);

    QList<QObject*> destroyed;
    QObject::connect(msi1, &QObject::destroyed, [&](QObject*) { destroyed << msi1; });
    QObject::connect(msi2, &QObject::destroyed, [&](QObject*) { destroyed << msi2; });
    QObject::connect(msi3, &QObject::destroyed, [&](QObject*) { destroyed << msi3; });

    app.reset();
    EXPECT_THAT(destroyed, UnorderedElementsAre(msi1, msi2, msi3));
}
