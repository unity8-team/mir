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
 */

#ifndef MOCK_SHARED_WAKELOCK_H
#define MOCK_SHARED_WAKELOCK_H

#include <Unity/Application/sharedwakelock.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockSharedWakelock : public qtmir::SharedWakelock
{
    MockSharedWakelock()
    {
        ON_CALL(*this, createWakelock()).WillByDefault(Invoke(this, &MockSharedWakelock::doCreateWakelock));
    }

    MOCK_METHOD0(createWakelock, QObject*());
    bool wakelockHeld() { return m_wakelock; }


    QObject* doCreateWakelock() const
    {
        return new QObject;
    }
};

} // namespace testing
#endif // MOCK_SHARED_WAKELOCK_H
