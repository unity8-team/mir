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

#ifndef UBUNTU_SCREEN_H
#define UBUNTU_SCREEN_H

#include <qpa/qplatformscreen.h>
#include <QSurfaceFormat>
#include <EGL/egl.h>

class QOrientationSensor;

class UbuntuScreen : public QObject, public QPlatformScreen
{
    Q_OBJECT
public:
    UbuntuScreen();
    virtual ~UbuntuScreen();

    // QObject methods.
    void customEvent(QEvent* event) override;

    // QPlatformScreen methods.
    QImage::Format format() const override { return mFormat; }
    int depth() const override { return mDepth; }
    QRect geometry() const override { return mGeometry; }
    QRect availableGeometry() const override { return mAvailableGeometry; }
    Qt::ScreenOrientation nativeOrientation() const override { return mNativeOrientation; }
    Qt::ScreenOrientation orientation() const override { return mCurrentOrientation; }

    // New methods.
    QSurfaceFormat surfaceFormat() const { return mSurfaceFormat; }
    EGLDisplay eglDisplay() const { return mEglDisplay; }
    EGLConfig eglConfig() const { return mEglConfig; }
    void toggleSensors(bool enable) const;

public Q_SLOTS:
    void onOrientationReadingChanged();

private:
    QRect mGeometry;
    QRect mAvailableGeometry;
    int mGridUnit;
    float mDensityPixelRatio;
    Qt::ScreenOrientation mNativeOrientation;
    Qt::ScreenOrientation mCurrentOrientation;
    QOrientationSensor* mOrientationSensor;
    QImage::Format mFormat;
    int mDepth;
    QSurfaceFormat mSurfaceFormat;
    EGLDisplay mEglDisplay;
    EGLConfig mEglConfig;
};

#endif // UBUNTU_SCREEN_H
