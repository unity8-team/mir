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

#ifndef MOCK_APPLICATION_CONTROLLER_H
#define MOCK_APPLICATION_CONTROLLER_H

#include <Unity/Application/applicationcontroller.h>

#include <core/posix/fork.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockApplicationController : public qtmir::ApplicationController
{
    MOCK_METHOD1(primaryPidForAppId, pid_t(const QString& appId));
    MOCK_METHOD2(appIdHasProcessId, bool(pid_t, const QString&));
    MOCK_CONST_METHOD1(findDesktopFileForAppId, QFileInfo(const QString &appId));

    MOCK_METHOD1(stopApplicationWithAppId, bool(const QString&));
    MOCK_METHOD2(startApplicationWithAppIdAndArgs, bool(const QString&, const QStringList&));
    MOCK_METHOD1(pauseApplicationWithAppId, bool(const QString&));
    MOCK_METHOD1(resumeApplicationWithAppId, bool(const QString&));

    MockApplicationController()
    {
        using namespace ::testing;
        ON_CALL(*this, primaryPidForAppId(_))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doPrimaryPidForAppId));

        ON_CALL(*this, appIdHasProcessId(_, _))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doAppIdHasProcessId));

        ON_CALL(*this, findDesktopFileForAppId(_))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doFindDesktopFileForAppId));

        ON_CALL(*this, stopApplicationWithAppId(_))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doStopApplicationWithAppId));

        ON_CALL(*this, startApplicationWithAppIdAndArgs(_, _))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doStartApplicationWithAppIdAndArgs));

        ON_CALL(*this, pauseApplicationWithAppId(_))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doPauseApplicationWithAppId));

        ON_CALL(*this, resumeApplicationWithAppId(_))
                .WillByDefault(
                    Invoke(this, &MockApplicationController::doResumeApplicationWithAppId));
    }

    pid_t doPrimaryPidForAppId(const QString& appId)
    {
        auto it = children.find(appId);
        if (it == children.end())
            return -1;

        return it->pid();
    }

    bool doAppIdHasProcessId(pid_t pid, const QString& appId)
    {
        auto it = children.find(appId);
        if (it == children.end())
            return -1;

        return it->pid() == pid;
    }

    QFileInfo doFindDesktopFileForAppId(const QString& appId) const
    {
        QString path = QString("/usr/share/applications/%1.desktop").arg(appId);
        return QFileInfo(path);
    }

    bool doStopApplicationWithAppId(const QString& appId)
    {
        Q_UNUSED(appId);

        return false;
    }

    bool doStartApplicationWithAppIdAndArgs(const QString& appId, const QStringList& args)
    {
        Q_UNUSED(args);

        auto child = core::posix::fork([]()
        {
            while (true);

            return core::posix::exit::Status::success;
        }, core::posix::StandardStream::empty);

        if (child.pid() > 0)
        {
            children.insert(appId, child);
            return true;
        }

        return false;
    }

    bool doPauseApplicationWithAppId(const QString& appId)
    {
        Q_UNUSED(appId);

        return false;
    }

    bool doResumeApplicationWithAppId(const QString& appId)
    {
        Q_UNUSED(appId);

        return false;
    }

    QMap<QString, core::posix::ChildProcess> children;
};
} // namespace testing

#endif // MOCK_APPLICATION_CONTROLLER_H
