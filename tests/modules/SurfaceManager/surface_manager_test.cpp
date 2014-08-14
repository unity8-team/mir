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

#include <QSignalSpy>

using namespace qtmir;

namespace ms = mir::scene;
namespace mtd = mir::test::doubles;

class SurfaceManagerTests : public ::testing::QtMirTest
{
public:
    SurfaceManagerTests()
    {}

    QList<MirSurfaceItem*> listPromptSurfaces(Application* app) {
        QList<MirSurfaceItem*> surfaces;
        app->foreachPromptSurface([&surfaces](MirSurfaceItem* surface) { surfaces << surface; });
        return surfaces;
    }

    uint countPromptSurfaces(Application* app) {
        uint count = 0;
        app->foreachPromptSurface([&count](MirSurfaceItem*) { count++; });
        return count;
    }
};

TEST_F(SurfaceManagerTests, TestPromptSession)
{
    using namespace testing;

    Application* application = startApplication(__LINE__, "test-app");
    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), application_for(_)).WillRepeatedly(Return(application->session()));
    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), helper_for(_)).WillRepeatedly(Return(nullptr));

    std::shared_ptr<ms::PromptSession> promptSession = std::make_shared<MockPromptSession>();
    std::shared_ptr<ms::Session> promptSceneSession = std::make_shared<MockSession>("test-app", __LINE__);
    std::shared_ptr<ms::Surface> promptSurface = std::make_shared<mtd::StubSceneSurface>(__LINE__);

    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), for_each_provider_in(promptSession,_)).WillRepeatedly(WithArgs<1>(Invoke(
        [&](std::function<void(std::shared_ptr<ms::Session> const& prompt_provider)> const& f) {
            f(promptSceneSession);
        })));

    surfaceManager.onSessionCreatedSurface(promptSceneSession.get(), promptSurface);
    QList<MirSurfaceItem*> promptSurfaces = surfaceManager.surfacesForSession(promptSceneSession.get());
    ASSERT_TRUE(promptSurfaces.count() > 0);

    applicationManager.onPromptSessionStarting(promptSession);
    surfaceManager.onPromptProviderAdded(promptSession.get(), promptSceneSession);

    // empty prompt surfaces
    EXPECT_THAT(listPromptSurfaces(application), ContainerEq(QList<MirSurfaceItem*>()));
    for (MirSurfaceItem* surface : promptSurfaces) {
        QMetaObject::invokeMethod(surface, "surfaceDamaged");
    }
    EXPECT_THAT(listPromptSurfaces(application), ContainerEq(promptSurfaces));

    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), for_each_provider_in(promptSession,_)).WillRepeatedly(InvokeWithoutArgs([]{}));

    QSignalSpy spyRemoved(promptSurfaces.at(0), SIGNAL(removed()));
    surfaceManager.onPromptProviderRemoved(promptSession.get(), promptSceneSession);
    EXPECT_EQ(spyRemoved.count(), 1);
}
