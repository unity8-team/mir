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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#ifndef TASKCONTROLLER_H
#define TASKCONTROLLER_H

#include <QObject>

#include "application.h"

// upstart
extern "C" {
    #include "upstart-app-launch.h"
}

class TaskController : public QObject
{
    Q_OBJECT
public:
    static TaskController* singleton();
    ~TaskController();

    bool start(const QString& appId, const QStringList& args);
    bool stop(const QString& appId);

    bool suspend(const QString& appId);
    bool resume(const QString& appId);

    bool appIdHasProcessId(const QString& appId, const quint64 pid);

Q_SIGNALS:
    void processStartReport(const QString& appId, const bool failure);
    void processStopped(const QString& appId, const bool unexpectedly);
    void requestFocus(const QString& appId);
    void requestResume(const QString& appId);

private:
    TaskController(QObject *parent = 0);

    static TaskController* m_theTaskController;
    upstart_app_launch_app_observer_t preStartCallback, startedCallback, stopCallback, focusCallback, resumeCallback;
    upstart_app_launch_app_failed_observer_t failureCallback;
};

#endif // TASKCONTROLLER_H
