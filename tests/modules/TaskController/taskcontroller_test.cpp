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

#include <Unity/Application/taskcontroller.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
using namespace qtmir;

struct TriggerableApplicationController : public ApplicationController
{
    void triggerApplicationStarted(const QString& appId)
    {
        Q_EMIT applicationStarted(appId);
    }

    void triggerApplicationStopped(const QString& appId)
    {
        Q_EMIT applicationStopped(appId);
    }

    void triggerApplicationFocusRequest(const QString& appId)
    {
        Q_EMIT applicationFocusRequest(appId);
    }

    MOCK_METHOD1(primaryPidForAppId, pid_t (const QString&));
    MOCK_METHOD2(appIdHasProcessId, bool(pid_t, const QString&));
    MOCK_CONST_METHOD1(findDesktopFileForAppId, QFileInfo(const QString &appId));

    MOCK_METHOD1(stopApplicationWithAppId, bool(const QString&));
    MOCK_METHOD2(startApplicationWithAppIdAndArgs, bool(const QString&, const QStringList&));
    MOCK_METHOD1(pauseApplicationWithAppId, bool(const QString&));
    MOCK_METHOD1(resumeApplicationWithAppId, bool(const QString&));
};
}

TEST(TaskController, startingAnApplicationCallsCorrectlyIntoApplicationController)
{
    using namespace ::testing;

    const QString appId{"com.canonical.does.not.exist"};

    testing::NiceMock<TriggerableApplicationController> appController;
    QSharedPointer<TriggerableApplicationController> appControllerPtr(
                &appController,
                [](ApplicationController*){});

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, QStringList())).Times(1).WillRepeatedly(Return(true));

    TaskController taskController(nullptr,
                                  appControllerPtr);

    taskController.start(appId, QStringList());
}

TEST(TaskController, stoppingAnApplicationCallsCorrectlyIntoApplicationController)
{
    using namespace ::testing;

    const QString appId{"com.canonical.does.not.exist"};

    testing::NiceMock<TriggerableApplicationController> appController;
    QSharedPointer<TriggerableApplicationController> appControllerPtr(
                &appController,
                [](ApplicationController*){});

    EXPECT_CALL(appController, stopApplicationWithAppId(appId)).Times(1).WillRepeatedly(Return(true));

    TaskController taskController(nullptr,
                                  appControllerPtr);

    taskController.stop(appId);
}

