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

#ifndef QT_MIR_TEST_FRAMEWORK_H
#define QT_MIR_TEST_FRAMEWORK_H

#include <gtest/gtest.h>

#include <Unity/Application/application_manager.h>
#include <Unity/Application/applicationcontroller.h>
#include <Unity/Application/mirsurfacemanager.h>
#include <Unity/Application/sessionmanager.h>
#include <Unity/Application/sharedwakelock.h>
#include <Unity/Application/taskcontroller.h>
#include <Unity/Application/proc_info.h>
#include <mirserver.h>

#include "mock_application_controller.h"
#include "mock_desktop_file_reader.h"
#include "mock_proc_info.h"
#include "mock_mir_session.h"
#include "mock_focus_controller.h"
#include "mock_prompt_session_manager.h"
#include "mock_prompt_session.h"

namespace ms = mir::scene;
using namespace qtmir;

namespace qtmir {

// Initialization of mir::Server needed for by tests
class TestMirServerInit : virtual mir::Server
{
public:
    TestMirServerInit()
    {
        override_the_prompt_session_manager(
            [this]{ return the_mock_prompt_session_manager(); });
    }

    std::shared_ptr<mir::scene::MockPromptSessionManager> the_mock_prompt_session_manager()
    {
        return mock_prompt_session_manager;
    }

private:
    typedef testing::NiceMock<mir::scene::MockPromptSessionManager> StubPromptSessionManager;
    std::shared_ptr<StubPromptSessionManager> const mock_prompt_session_manager
        {std::make_shared<StubPromptSessionManager>()};
};

class FakeMirServer: private TestMirServerInit, public MirServer
{
public:
    FakeMirServer()
    : MirServer(0, nullptr)
    {
    }

    using TestMirServerInit::the_mock_prompt_session_manager;
};

} // namespace qtmir

namespace testing {

class QtMirTest : public ::testing::Test
{
public:
    QtMirTest()
        : mirServer{
            QSharedPointer<FakeMirServer> (new FakeMirServer)
        }
        , taskController{
              QSharedPointer<TaskController> (
                  new TaskController(
                      nullptr,
                      QSharedPointer<ApplicationController>(
                          &appController,
                          [](ApplicationController*){})
                  )
              )
        }
        , sharedWakelock{
            QSharedPointer<SharedWakelock>(new SharedWakelock)
        }
        , applicationManager{
            mirServer,
            taskController,
            sharedWakelock,
            QSharedPointer<DesktopFileReader::Factory>(
                &desktopFileReaderFactory,
                [](DesktopFileReader::Factory*){}),
            QSharedPointer<ProcInfo>(&procInfo,[](ProcInfo *){})
        }
        , sessionManager{
            mirServer,
            &applicationManager,
        }
        , surfaceManager{
            mirServer,
            &sessionManager
        }
    {
    }

    Application* startApplication(quint64 procId, QString const& appId)
    {
        using namespace testing;

        ON_CALL(appController,appIdHasProcessId(procId, appId)).WillByDefault(Return(true));

        // Set up Mocks & signal watcher
        auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
        ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
        ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

        ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

        EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
                .Times(1)
                .WillOnce(Return(true));

        auto application = applicationManager.startApplication(appId, ApplicationManager::NoFlag);
        applicationManager.onProcessStarting(appId);

        bool authed = false;
        applicationManager.authorizeSession(procId, authed);
        EXPECT_EQ(authed, true);

        auto appSession = std::make_shared<mir::scene::MockSession>(appId.toStdString(), procId);
        sessionManager.onSessionStarting(appSession);
        return application;
        return nullptr;
    }

    testing::NiceMock<testing::MockApplicationController> appController;
    testing::NiceMock<testing::MockProcInfo> procInfo;
    testing::NiceMock<testing::MockDesktopFileReaderFactory> desktopFileReaderFactory;
    QSharedPointer<FakeMirServer> mirServer;
    QSharedPointer<TaskController> taskController;
    QSharedPointer<SharedWakelock> sharedWakelock;
    ApplicationManager applicationManager;
    SessionManager sessionManager;
    MirSurfaceManager surfaceManager;
};
} // namespace testing

#endif // QT_MIR_TEST_FRAMEWORK_H
