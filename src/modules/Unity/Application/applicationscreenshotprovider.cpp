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

// Qt
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

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
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider::requestImage - imageId=" << imageId;

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

    QImage screenshotImage;
    QMutex screenshotMutex;
    QWaitCondition screenshotTakenCondition;

    app->session()->take_snapshot(
        [&](mir::scene::Snapshot const& snapshot)
        {
            qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider - Mir snapshot ready with size"
                                        << snapshot.size.height.as_int() << "x" << snapshot.size.width.as_int();

            {
                // since we mirror, no need to offset starting position of the pixels
                QImage fullSizeScreenshot = QImage( (const uchar*)snapshot.pixels,
                            snapshot.size.width.as_int(),
                            snapshot.size.height.as_int(),
                            QImage::Format_ARGB32_Premultiplied).mirrored();

                QMutexLocker screenshotMutexLocker(&screenshotMutex);

                if (requestedSize.isValid()) {
                    *size = requestedSize.boundedTo(fullSizeScreenshot.size());
                    screenshotImage = fullSizeScreenshot.scaled(*size, Qt::IgnoreAspectRatio,
                        Qt::SmoothTransformation);
                } else {
                    *size = fullSizeScreenshot.size();
                    screenshotImage = fullSizeScreenshot;
                }

                screenshotTakenCondition.wakeAll();
            }
        });

    {
        QMutexLocker screenshotMutexLocker(&screenshotMutex);
        if (screenshotImage.isNull()) {
            // mir is taking a snapshot in a separate thread. Wait here until it's done.
            screenshotTakenCondition.wait(&screenshotMutex);
        } else {
            // mir took a snapshot synchronously or it was asynchronous but already finished.
            // In either case, there's no need to wait.
        }
    }

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider - working with size" << screenshotImage;

    return screenshotImage;
}

} // namespace qtmir
