/*
 * Copyright (C) 2015 Canonical, Ltd.
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "qtmir_test.h"


using namespace qtmir;

class ApplicationTests : public ::testing::QtMirTest
{
public:
    ApplicationTests()
    {}
};

TEST_F(ApplicationTests, checkFocusAquiresWakeLock)
{
    using namespace ::testing;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1);

    startApplication(123, "app");
    applicationManager.focusApplication("app");
}

TEST_F(ApplicationTests, checkSuspendReleasesWakeLock)
{
    using namespace ::testing;

    auto app = startApplication(123, "app");
    auto session = app->session();

    applicationManager.focusApplication("app");

    Q_EMIT session->suspended();
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}

TEST_F(ApplicationTests, checkResumeAcquiresWakeLock)
{
    using namespace ::testing;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1);

    auto app = startApplication(123, "app");
    auto session = app->session();

    Q_EMIT session->resumed();
}

TEST_F(ApplicationTests, checkRespawnAcquiresWakeLock)
{
    using namespace ::testing;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1);
    const QString appId = "app";

    auto app = startApplication(123, "app");

    // as respawn fires startApplicationWithAppIdAndArgs again, keep gmock quiet about another call
    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
            .Times(1)
            .WillRepeatedly(Return(true));

    // respawn by setting app state as Stopped, delete the Session associated, then set to Running state
    app->setState(Session::State::Stopped);
    delete app->session();
    app->setState(Session::State::Running);
}

TEST_F(ApplicationTests, checkDashFocusDoesNotAquireWakeLock)
{
    using namespace ::testing;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(0);

    startApplication(123, "unity8-dash");
    applicationManager.focusApplication("unity8-dash");
}

TEST_F(ApplicationTests, checkDashSuspendDoesNotImpactWakeLock)
{
    using namespace ::testing;

    auto app = startApplication(123, "unity8-dash");
    auto session = app->session();

    applicationManager.focusApplication("unity8-dash");

    Q_EMIT session->suspended();
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}

TEST_F(ApplicationTests, checkDashResumeDoesNotAcquireWakeLock)
{
    using namespace ::testing;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(0);

    auto app = startApplication(123, "unity8-dash");
    auto session = app->session();

    Q_EMIT session->resumed();
}

