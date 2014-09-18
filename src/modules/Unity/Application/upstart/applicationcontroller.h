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

#ifndef UPSTART_APPLICATION_CONTROLLER_H
#define UPSTART_APPLICATION_CONTROLLER_H

#include "../applicationcontroller.h"

namespace qtmir
{
namespace upstart
{

class ApplicationController : public qtmir::ApplicationController
{
public:
    ApplicationController();
    ~ApplicationController();

    pid_t primaryPidForAppId(const QString& appId) override;
    bool appIdHasProcessId(pid_t pid, const QString& appId) override;

    bool stopApplicationWithAppId(const QString& appId) override;
    bool startApplicationWithAppIdAndArgs(const QString& appId, const QStringList& arguments) override;

    bool pauseApplicationWithAppId(const QString& appId) override;
    bool resumeApplicationWithAppId(const QString& appId) override;

    QFileInfo findDesktopFileForAppId(const QString &appId) const override;

private:
    struct Private;
    QScopedPointer<Private> impl;
};

} // namespace upstart
} // namespace qtmir

#endif // UPSTART_APPLICATION_CONTROLLER_H
