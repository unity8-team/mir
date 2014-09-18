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

#include "applicationcontroller.h"

// qtmir
#include <logging.h>

// Qt
#include <QStandardPaths>

// upstart
extern "C" {
    #include "ubuntu-app-launch.h"
}

namespace qtmir
{
namespace upstart
{

struct ApplicationController::Private
{
    UbuntuAppLaunchAppObserver preStartCallback = nullptr;
    UbuntuAppLaunchAppObserver startedCallback = nullptr;
    UbuntuAppLaunchAppObserver stopCallback = nullptr;
    UbuntuAppLaunchAppObserver focusCallback = nullptr;
    UbuntuAppLaunchAppObserver resumeCallback = nullptr;
    UbuntuAppLaunchAppFailedObserver failureCallback = nullptr;
};

namespace {
/**
 * @brief toShortAppIdIfPossible
 * @param appId - any string that you think is an appId
 * @return if a valid appId was input, a shortened appId is returned, else returns the input string unaltered
 */
QString toShortAppIdIfPossible(const QString &appId) {
    gchar *package, *application;
    if (ubuntu_app_launch_app_id_parse(appId.toLatin1().constData(), &package, &application, nullptr)) {
        // is long appId, so assemble its short appId
        QString shortAppId = QString("%1_%2").arg(package).arg(application);
        g_free(package);
        g_free(application);
        return shortAppId;
    } else {
        return appId;
    }
}

/**
 * @brief toLongAppIdIfPossible
 * @param shortAppId - any string that you think is a short appId
 * @return if valid short appId was input, the corresponding long appId is returned. If a long appId was
 * entered, it is returned unchanged. Anything else is also returned unchanged.
 */
QString toLongAppIdIfPossible(const QString &shortAppId) {
    if (ubuntu_app_launch_app_id_parse(shortAppId.toLatin1().constData(), nullptr, nullptr, nullptr)) {
        // then we got a long appId after all, just return it
        return shortAppId;
    } else {
        // try to parse the string in the form "$package_$application"
        QRegExp shortAppIdMask("[a-z0-9][a-z0-9+.-]+_[a-zA-Z0-9+.-]+");
        if (!shortAppIdMask.exactMatch(shortAppId)) {
            // input string not a short appId, so just return it unchanged
            return shortAppId;
        }

        // ask upstart for the long appId corresponding to this short appId
        QStringList parts = shortAppId.split("_");
        gchar *longAppId;
        longAppId = ubuntu_app_launch_triplet_to_app_id(parts.first().toLatin1().constData(),
                                                         parts.last().toLatin1().constData(),
                                                         nullptr);
        if (longAppId == nullptr) {
            // was unable to construct a long appId from the short appId, return input unchanged
            return shortAppId;
        } else {
            QString appId(longAppId);
            g_free(longAppId);
            return appId;
        }
    }
}

} // namespace

ApplicationController::ApplicationController()
    : qtmir::ApplicationController(),
      impl(new Private())
{
    impl->preStartCallback = [](const gchar * appId, gpointer userData) {
        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationAboutToBeStarted(toShortAppIdIfPossible(appId)));
    };

    impl->startedCallback = [](const gchar * appId, gpointer userData) {
        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationStarted(toShortAppIdIfPossible(appId)));
    };

    impl->stopCallback = [](const gchar * appId, gpointer userData) {
        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationStopped(toShortAppIdIfPossible(appId)));
    };

    impl->focusCallback = [](const gchar * appId, gpointer userData) {
        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationFocusRequest(toShortAppIdIfPossible(appId)));
    };

    impl->resumeCallback = [](const gchar * appId, gpointer userData) {
        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationResumeRequest(toShortAppIdIfPossible(appId)));
    };

    impl->failureCallback = [](const gchar * appId, UbuntuAppLaunchAppFailed failureType, gpointer userData) {
        ApplicationController::Error error;
        switch(failureType)
        {
        case UBUNTU_APP_LAUNCH_APP_FAILED_CRASH: error = ApplicationController::Error::APPLICATION_CRASHED;
        case UBUNTU_APP_LAUNCH_APP_FAILED_START_FAILURE: error = ApplicationController::Error::APPLICATION_FAILED_TO_START;
        }

        auto thiz = static_cast<ApplicationController*>(userData);
        Q_EMIT(thiz->applicationError(toShortAppIdIfPossible(appId), error));
    };

    ubuntu_app_launch_observer_add_app_starting(impl->preStartCallback, this);
    ubuntu_app_launch_observer_add_app_started(impl->startedCallback, this);
    ubuntu_app_launch_observer_add_app_stop(impl->stopCallback, this);
    ubuntu_app_launch_observer_add_app_focus(impl->focusCallback, this);
    ubuntu_app_launch_observer_add_app_resume(impl->resumeCallback, this);
    ubuntu_app_launch_observer_add_app_failed(impl->failureCallback, this);
}

ApplicationController::~ApplicationController()
{
    ubuntu_app_launch_observer_delete_app_starting(impl->preStartCallback, this);
    ubuntu_app_launch_observer_delete_app_started(impl->startedCallback, this);
    ubuntu_app_launch_observer_delete_app_stop(impl->stopCallback, this);
    ubuntu_app_launch_observer_delete_app_focus(impl->focusCallback, this);
    ubuntu_app_launch_observer_delete_app_resume(impl->resumeCallback, this);
    ubuntu_app_launch_observer_delete_app_failed(impl->failureCallback, this);
}

pid_t ApplicationController::primaryPidForAppId(const QString& appId)
{
    GPid pid = ubuntu_app_launch_get_primary_pid(toLongAppIdIfPossible(appId).toLatin1().constData());
    if (!pid)
        qDebug() << "ApplicationController::primaryPidForAppId FAILED to get PID for appId=" << appId;

    return pid;
}

bool ApplicationController::appIdHasProcessId(pid_t pid, const QString& appId)
{
    return ubuntu_app_launch_pid_in_app_id(pid, toLongAppIdIfPossible(appId).toLatin1().constData());
}

bool ApplicationController::stopApplicationWithAppId(const QString& appId)
{
    auto result = ubuntu_app_launch_stop_application(toLongAppIdIfPossible(appId).toLatin1().constData());
    if (!result)
        qDebug() << "ApplicationController::stopApplication FAILED to stop appId=" << appId;

    return result;
}

bool ApplicationController::startApplicationWithAppIdAndArgs(const QString& appId, const QStringList& arguments)
{
    // Convert arguments QStringList into format suitable for ubuntu-app-launch
    // The last item should be null, which is done by g_new0, we just don't fill it.
    auto upstartArgs = g_new0(gchar *, arguments.length() + 1);

    for (int i=0; i<arguments.length(); i++) {
        upstartArgs[i] = g_strdup(arguments.at(i).toUtf8().data());
    }

    auto result = ubuntu_app_launch_start_application(
                toLongAppIdIfPossible(appId).toLatin1().constData(),
                static_cast<const gchar * const *>(upstartArgs));

    g_strfreev(upstartArgs);

    if (!result)
        qDebug() << "Application::Controller::startApplicationWithAppIdAndArgs FAILED to start appId" << appId;

    return result;
}

bool ApplicationController::pauseApplicationWithAppId(const QString& appId)
{
    auto result = ubuntu_app_launch_pause_application(toLongAppIdIfPossible(appId).toLatin1().constData());
    if (!result)
        qDebug() << "ApplicationController::pauseApplication FAILED to stop appId=" << appId;

    return result;
}

bool ApplicationController::resumeApplicationWithAppId(const QString& appId)
{
    auto result = ubuntu_app_launch_resume_application(toLongAppIdIfPossible(appId).toLatin1().constData());
    if (!result)
        qDebug() << "ApplicationController::resumeApplication FAILED to stop appId=" << appId;

    return result;
}


QFileInfo ApplicationController::findDesktopFileForAppId(const QString &appId) const
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationController::desktopFilePathForAppId - appId=" << appId;

    // Search for the correct desktop file using a simple heuristic
    int dashPos = -1;
    QString helper = toLongAppIdIfPossible(appId);
    QString desktopFile;

    do {
        if (dashPos != -1) {
            helper = helper.replace(dashPos, 1, '/');
        }

        desktopFile = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QString("%1.desktop").arg(helper));
        if (!desktopFile.isEmpty()) return desktopFile;

        dashPos = helper.indexOf("-");
    } while (dashPos != -1);

    return QFileInfo();
}

} // namespace upstart
} // namespace qtmir
