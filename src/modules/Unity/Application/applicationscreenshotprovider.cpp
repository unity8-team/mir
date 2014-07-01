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
 */

// local
#include "applicationscreenshotprovider.h"
#include "application_manager.h"
#include "application.h"

// QPA-mirserver
#include "logging.h"

// mir
#include <mir/scene/session.h>

namespace qtmir
{

ApplicationScreenshotProvider::ApplicationScreenshotProvider(ApplicationManager *appManager)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_appManager(appManager)
{
}

QImage ApplicationScreenshotProvider::requestImage(const QString &imageId, QSize *size,
                                                   const QSize &requestedSize)
{
    // We ignore requestedSize here intentionally to avoid keeping scaled copies around
    Q_UNUSED(requestedSize)

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider::requestImage - appId=" << imageId;

    QString appId = imageId.split('/').first();

    Application* app = static_cast<Application*>(m_appManager->findApplication(appId));
    if (app == nullptr) {
        qWarning() << "ApplicationScreenshotProvider - app with appId" << appId << "not found";
        return QImage();
    }

    // TODO: if app not ready, return an app-provided splash image. If app has been stopped with saved state
    // return the screenshot that was saved to disk.
    if (!app->session() || !app->session()->default_surface()) {
        qWarning() << "ApplicationScreenshotProvider - app session not found - asking for screenshot too early";
        return QImage();
    }

    QImage image = app->screenshotImage();

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider - working with size" << image;
    size->setWidth(image.width());
    size->setHeight(image.height());

    return image;
}

} // namespace qtmir
