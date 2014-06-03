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

#include "ubuntubackingstore.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>

UbuntuBackingStore::UbuntuBackingStore(QWindow* window)
    : QPlatformBackingStore(window)
    , mContext(new QOpenGLContext)
{
    mContext->setFormat(window->requestedFormat());
    mContext->setScreen(window->screen());
    mContext->create();
}

UbuntuBackingStore::~UbuntuBackingStore()
{
    delete mContext;
}

void UbuntuBackingStore::flush(QWindow* window, const QRegion& region, const QPoint& offset)
{
    Q_UNUSED(region);
    Q_UNUSED(offset);
    mContext->swapBuffers(window);
}

void UbuntuBackingStore::beginPaint(const QRegion& region)
{
    Q_UNUSED(region);
    window()->setSurfaceType(QSurface::OpenGLSurface);
    mContext->makeCurrent(window());
    mDevice = new QOpenGLPaintDevice(window()->size());
}

void UbuntuBackingStore::endPaint()
{
    delete mDevice;
}

void UbuntuBackingStore::resize(const QSize& size, const QRegion& staticContents)
{
    Q_UNUSED(size);
    Q_UNUSED(staticContents);
}

QPaintDevice* UbuntuBackingStore::paintDevice()
{
    return reinterpret_cast<QPaintDevice*>(mDevice);
}
