/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

// local
#include "taskcontroller.h"
#include "logging.h"

// Qt
#include <QStringList>

// Process C++
#include <core/posix/process.h>
#include <core/posix/this_process.h>
#include <core/posix/linux/proc/process/oom_adj.h>
#include <core/posix/linux/proc/process/oom_score_adj.h>

// STL
#include <mutex>

// std
#include <csignal>
#include <unistd.h>

namespace qtmir
{

TaskController::TaskController(
        QObject *parent,
        const QSharedPointer<ApplicationController> &appController,
        const QSharedPointer<ProcessController> &processController) :
    QObject(parent),
    m_appController(appController),
    m_processController(processController)
{
    connect(m_appController.data(),
            &ApplicationController::applicationAboutToBeStarted,
            this,
            &TaskController::processStarting);

    connect(m_appController.data(),
            &ApplicationController::applicationStarted,
            this,
            &TaskController::onApplicationStarted);

    connect(m_appController.data(),
            &ApplicationController::applicationStopped,
            this,
            &TaskController::processStopped);

    connect(m_appController.data(),
            &ApplicationController::applicationFocusRequest,
            this,
            &TaskController::onApplicationFocusRequest);

    connect(m_appController.data(),
            &ApplicationController::applicationResumeRequest,
            this,
            &TaskController::onApplicationResumeRequest);

    connect(m_appController.data(),
            &ApplicationController::applicationError,
            this,
            &TaskController::onApplicationError);
}

TaskController::~TaskController()
{
}

bool TaskController::start(const QString& appId, const QStringList& arguments)
{
    qCDebug(QTMIR_APPLICATIONS) << "TaskController::start - appId=" << appId;
    return m_appController->startApplicationWithAppIdAndArgs(appId, arguments);
}

bool TaskController::stop(const QString& appId)
{
    qCDebug(QTMIR_APPLICATIONS) << "TaskController::stop - appId=" << appId;
    auto result = m_appController->stopApplicationWithAppId(appId);
    if (!result)
        qCDebug(QTMIR_APPLICATIONS) << "TaskController::stopApplication - FAILED to stop appId=" << appId;

    return result;
}

bool TaskController::appIdHasProcessId(const QString& appId, const quint64 pid) const
{
    qCDebug(QTMIR_APPLICATIONS) << "TaskController::isApplicationPid - appId=" << appId << "pid=" << pid;
    return m_appController->appIdHasProcessId(pid, appId);
}

QFileInfo TaskController::findDesktopFileForAppId(const QString &appId) const
{
    return m_appController->findDesktopFileForAppId(appId);
}

bool TaskController::suspend(Application* app)
{
    qCDebug(QTMIR_APPLICATIONS) << "TaskController::suspend - appId=" << app->appId();
    pid_t pid = m_appController->primaryPidForAppId(app->appId());
    if (pid == 0) {
        pid = app->pid();
    }
    m_processController->oomController()->ensureProcessLikelyToBeKilled(pid);

    if (pid) {
        // We do assume that the app was launched by ubuntu-app-launch and with that,
        // in its own process group. For that, we interpret the pid as pgid and
        // sigstop the complete process group on suspend.
        return m_processController->sigStopProcessGroupForPid(pid);
    } else {
        return false;
    }
}

bool TaskController::resume(Application* app)
{
    qCDebug(QTMIR_APPLICATIONS) << "TaskController::resume - appId=" << app->appId();
    pid_t pid = m_appController->primaryPidForAppId(app->appId());
    if (pid == 0) {
        pid = app->pid();
    }

    m_processController->oomController()->ensureProcessUnlikelyToBeKilled(pid);

    if (pid) {
        // We do assume that the app was launched by ubuntu-app-launch and with that,
        // in its own process group. For that, we interpret the pid as pgid and
        // sigcont the complete process group on resume.
        return m_processController->sigContinueProcessGroupForPid(pid);
        return true;
    } else {
        qCDebug(QTMIR_APPLICATIONS) << "TaskController::resume - couldn't find PID to resume for appId=" << app->appId();
        return false;
    }
}

void TaskController::onApplicationStarted(const QString& id)
{
    pid_t pid = m_appController->primaryPidForAppId(id);
    m_processController->oomController()->ensureProcessUnlikelyToBeKilled(pid);
}

void TaskController::onApplicationFocusRequest(const QString& id)
{
    pid_t pid = m_appController->primaryPidForAppId(id);
    m_processController->oomController()->ensureProcessUnlikelyToBeKilled(pid);
    Q_EMIT requestFocus(id);
}

void TaskController::onApplicationResumeRequest(const QString& id)
{
    Q_EMIT requestResume(id);
}

void TaskController::onApplicationError(const QString& id, ApplicationController::Error error)
{
    Q_EMIT processFailed(id, (error == ApplicationController::Error::APPLICATION_FAILED_TO_START) );
}

} // namespace qtmir
