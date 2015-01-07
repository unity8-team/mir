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
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#ifndef WAKELOCK_H
#define WAKELOCK_H

#include <QObject>
#include <QSet>

namespace qtmir {

class Wakelock;

class SharedWakelock : public QObject
{
    Q_OBJECT
public:
    SharedWakelock() noexcept;
    virtual ~SharedWakelock() noexcept;

    void acquire(const QObject *caller);
    void release(const QObject *caller);

protected:
    Wakelock *m_wakelock;
    QSet<const QObject *> m_owners;

private:
    Q_DISABLE_COPY(SharedWakelock)
};

} // namespace qtmir

#endif // WAKELOCK_H
