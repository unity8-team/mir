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
#include "mirserver/mir/shell/application_session.h"

// fallback grid unit used if GRID_UNIT_PX is not in the environment.
const int defaultGridUnitPx = 8;

ApplicationScreenshotProvider::ApplicationScreenshotProvider(ApplicationManager *appManager)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_appManager(appManager)
    , m_panelHeight(54)
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

    m_panelHeight = 3 * gridUnitPx + 2 * densityPixelPx;
}

QQmlImageProviderBase::Flags ApplicationScreenshotProvider::flags() const
{
    // Force image fetching to always be async, prevent blocking main thread
    return QQmlImageProviderBase::ForceAsynchronousImageLoading;
}

QImage ApplicationScreenshotProvider::requestImage(const QString &appId, QSize * size,
                                                     const QSize &requestedSize)
{
    DLOG("ApplicationScreenshotProvider::requestPixmap (this=%p, id=%s)", this, appId.toLatin1().constData());

    Application* app = static_cast<Application*>(m_appManager->findApplication(appId));
    if (app == NULL) {
        LOG("ApplicationScreenshotProvider - app with appId %s not found", appId.toLatin1().constData());
        return QImage();
    }

    // TODO: if app not ready, return an app-provided splash image. If app has been stopped with saved state
    // return the screenshot that was saved to disk.
    if (!app->session() || !app->session()->default_surface()) {
        LOG("ApplicationScreenshotProvider - app session not found - taking screenshot too early");
        return QImage();
    }

    if (app->state() == Application::Stopped || app->state() == Application::Starting) {
        LOG("ApplicationScreenshotProvider -  unable to take screenshot of stopped/not-ready applications");
        return QImage();
    }

    /* Workaround for bug https://bugs.launchpad.net/qtubuntu/+bug/1209216 - currently all qtubuntu
     * based applications are allocated a fullscreen Mir surface, but draw in a geometry excluding
     * the panel's rectangle. Mir snapshots thus have a white rectangle which the panel hides.
     * So need to clip this rectangle from the snapshot. */
    int yOffset = 0;
    if (!app->fullscreen()) {
        yOffset = m_panelHeight;
    }

    QMutex mutex;
    QWaitCondition wait;
    mutex.lock();

    QImage image;

    app->session()->take_snapshot(
        [&](mir::shell::Snapshot const& snapshot)
        {
            DLOG("ApplicationScreenshotProvider - Mir snapshot ready with size %d x %d",
                 snapshot.size.height.as_int(), snapshot.size.width.as_int());

            image = QImage( (const uchar*)snapshot.pixels, // since we mirror, no need to offset starting position
                            snapshot.size.width.as_int(),
                            snapshot.size.height.as_int() - yOffset,
                            QImage::Format_ARGB32_Premultiplied).mirrored();
            wait.wakeOne();
        });

    wait.wait(&mutex, 5000);

    DLOG("ApplicationScreenshotProvider - working with size %d x %d", image.width(), image.height());
    size->setWidth(image.width());
    size->setHeight(image.height());

    if (requestedSize.isValid()) {
        image = image.scaled(requestedSize);
    }
    mutex.unlock();
    return image;
}
