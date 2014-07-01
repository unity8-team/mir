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

#ifndef APPLICATION_CONTROLLER_H
#define APPLICATION_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>

namespace qtmir
{

class ApplicationController : public QObject
{
    Q_OBJECT

public:
    enum class Error
    {
        APPLICATION_CRASHED,
        APPLICATION_FAILED_TO_START
    };

    ApplicationController(const ApplicationController&) = delete;
    virtual ~ApplicationController() = default;

    ApplicationController& operator=(const ApplicationController&) = delete;

    virtual pid_t primaryPidForAppId(const QString& appId) = 0;
    virtual bool appIdHasProcessId(pid_t pid, const QString& appId) = 0;

    virtual bool stopApplicationWithAppId(const QString& appId) = 0;
    virtual bool startApplicationWithAppIdAndArgs(const QString& appId, const QStringList& arguments) = 0;

    virtual QFileInfo findDesktopFileForAppId(const QString &appId) const = 0;

Q_SIGNALS:
    void applicationAboutToBeStarted(QString id);
    void applicationStarted(QString id);
    void applicationStopped(QString id);
    void applicationFocusRequest(QString id);
    void applicationResumeRequest(QString id);

    void applicationError(QString id, ApplicationController::Error error);

protected:
    ApplicationController() = default;
};

} // namespace qtmir

#endif // APPLICATION_CONTROLLER_H
