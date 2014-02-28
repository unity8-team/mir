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
 */

// local
#include "applicationscreenshotprovider.h"
#include "application_manager.h"
#include "application.h"

// unity-mir
#include "logging.h"

// mir
#include "mir/shell/session.h"

// fallback grid unit used if GRID_UNIT_PX is not in the environment.
const int defaultGridUnitPx = 8;

ApplicationScreenshotProvider::ApplicationScreenshotProvider(ApplicationManager *appManager)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_appManager(appManager)
{
    // See below to explain why this is needed for now.
    int gridUnitPx = defaultGridUnitPx;

    QByteArray gridUnitString = qgetenv("GRID_UNIT_PX");
    if (!gridUnitString.isEmpty()) {
        bool ok;
        int value = gridUnitString.toInt(&ok);
        if (ok) {
            gridUnitPx = value;
        }
    }

    int densityPixelPx = qFloor( (float)gridUnitPx / defaultGridUnitPx );
}

QImage ApplicationScreenshotProvider::requestImage(const QString &imageId, QSize * size,
                                                     const QSize &requestedSize)
{
    // We ignore requestedSize here intentionally to avoid keeping scaled copies around
    Q_UNUSED(requestedSize)

    DLOG("ApplicationScreenshotProvider::requestImage (this=%p, id=%s)", this, imageId.toLatin1().constData());

    QString appId = imageId.split('/').first();

    Application* app = static_cast<Application*>(m_appManager->findApplication(appId));
    if (app == NULL) {
        LOG("ApplicationScreenshotProvider - app with appId %s not found", appId.toLatin1().constData());
        return QImage();
    }

    // TODO: if app not ready, return an app-provided splash image. If app has been stopped with saved state
    // return the screenshot that was saved to disk.
    if (!app->session() || !app->session()->default_surface()) {
        LOG("ApplicationScreenshotProvider - app session not found - asking for screenshot too early");
        return QImage();
    }

    QImage image = app->screenshotImage();

    DLOG("ApplicationScreenshotProvider - working with size %d x %d", image.width(), image.height());
    size->setWidth(image.width());
    size->setHeight(image.height());

    return image;
}
