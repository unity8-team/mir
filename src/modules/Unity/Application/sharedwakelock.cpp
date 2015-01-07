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

#include "sharedwakelock.h"
#include "logging.h"

#include <QFile>
#include <QTextStream>

namespace qtmir {

static const char wakelockPath[] = "/sys/power/wake_lock";
static const char wakeunlockPath[] = "/sys/power/wake_unlock";
static const char wakelockString[] = "qtmir";

class Wakelock
{
public:
    Wakelock() noexcept {
        write(wakelockPath);
        qCDebug(QTMIR_APPLICATIONS) << "Wakelock acquired";
    }
    virtual ~Wakelock() noexcept {
        write(wakeunlockPath);
        qCDebug(QTMIR_APPLICATIONS) << "Wakelock released";
    }

private:
    void write(const char path[])
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QTextStream out(&file);
        out << wakelockString;
    }

    Q_DISABLE_COPY(Wakelock)
};

/**
 * @brief SharedWakelock - allow a single wakelock instance to be shared between multiple owners
 *
 * QtMir has application management duties to perform even if display is off. To prevent device
 * going to deeep sleep before QtMir is ready, have QtMir register a system wakelock when it needs to.
 *
 * This class allows multiple objects to own the wakelock simultaneously. The wakelock is first
 * registered when acquire has been called by one caller. Multiple callers may then share the
 * wakelock. The wakelock is only destroyed when all callers have called release.
 *
 * Note a caller cannot have multiple shares of the wakelock. Multiple calls to acquire are ignored.
 */

SharedWakelock::SharedWakelock() noexcept
    : m_wakelock(nullptr)
{
}

SharedWakelock::~SharedWakelock() noexcept
{
    if (m_wakelock)
        delete m_wakelock;
}


void SharedWakelock::acquire(const QObject *caller)
{
    if (m_owners.contains(caller)) {
        return;
    }

    // register a slot to remove itself from owners list if destroyed
    QObject::connect(caller, &QObject::destroyed, this, [&](QObject *caller) {
        release(caller);
    });

    if (m_wakelock == nullptr) {
        m_wakelock = new Wakelock;
    }

    m_owners.insert(caller);
}

void SharedWakelock::release(const QObject *caller)
{
    if (!m_owners.remove(caller)) {
        return;
    }

    if (m_owners.empty() && m_wakelock) {
        delete m_wakelock;
        m_wakelock = nullptr;
    }
}

} // namespace qtmir
