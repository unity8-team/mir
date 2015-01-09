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

#include <Unity/Application/sharedwakelock.h>

#include "mock_shared_wakelock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace qtmir;
using testing::MockSharedWakelock;

TEST(SharedWakelock, acquireCreatesAWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1);
    sharedWakelock.acquire(app1.data());
}

TEST(SharedWakelock, acquireThenReleaseDestroysTheWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);

    sharedWakelock.acquire(app1.data());
    sharedWakelock.release(app1.data());
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}

TEST(SharedWakelock, doubleAcquireBySameOwnerOnlyCreatesASingleWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app1.data());
}

TEST(SharedWakelock, doubleAcquireThenReleaseBySameOwnerDestroysWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app1.data());
    sharedWakelock.release(app1.data());
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}

TEST(SharedWakelock, acquireByDifferentOwnerOnlyCreatesASingleWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);
    QScopedPointer<QObject> app2(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app2.data());
}

TEST(SharedWakelock, twoOwnersWhenOneReleasesStillHoldWakelock)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);
    QScopedPointer<QObject> app2(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app2.data());
    sharedWakelock.release(app1.data());
    EXPECT_TRUE(sharedWakelock.wakelockHeld());
}

TEST(SharedWakelock, twoOwnersWhenBothReleaseWakelockReleased)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);
    QScopedPointer<QObject> app2(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app2.data());
    sharedWakelock.release(app2.data());
    sharedWakelock.release(app1.data());
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}

TEST(SharedWakelock, doubleReleaseOfSingleOwnerIgnored)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);
    QScopedPointer<QObject> app2(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    sharedWakelock.acquire(app2.data());
    sharedWakelock.release(app1.data());
    EXPECT_TRUE(sharedWakelock.wakelockHeld());

    sharedWakelock.release(app1.data());
    EXPECT_TRUE(sharedWakelock.wakelockHeld());
}

TEST(SharedWakelock, nullOwnerAcquireIgnored)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(0);
    sharedWakelock.acquire(nullptr);
}

TEST(SharedWakelock, nullOwnerReleaseIgnored)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(0);
    sharedWakelock.release(nullptr);
}

TEST(SharedWakelock, ifOwnerDestroyedWakelockReleased)
{
    using namespace ::testing;

    testing::NiceMock<MockSharedWakelock> sharedWakelock;
    QScopedPointer<QObject> app1(new QObject);

    EXPECT_CALL(sharedWakelock, createWakelock()).Times(1).WillOnce(Return(new QObject));
    sharedWakelock.acquire(app1.data());
    app1.reset();
    EXPECT_FALSE(sharedWakelock.wakelockHeld());
}
