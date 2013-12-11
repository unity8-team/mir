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

#ifndef UBUNTU_WINDOW_H
#define UBUNTU_WINDOW_H

#include <qpa/qplatformwindow.h>

#include <EGL/egl.h>

// Platform API
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/window.h>

class UbuntuInput;
class UbuntuScreen;

class UbuntuWindow : public QPlatformWindow
{
public:
    UbuntuWindow(QWindow* w, UbuntuScreen* screen,
                 UbuntuInput* input, UApplicationInstance* instance);
    virtual ~UbuntuWindow();

    // QPlatformWindow methods.
    WId winId() const override { return mId; }
    void setGeometry(const QRect&) override;
    void setWindowState(Qt::WindowState state) override;
    void setVisible(bool visible) override;

    // New methods.
    void createEGLSurface(EGLNativeWindowType nativeWindow);
    void destroyEGLSurface();
    EGLSurface eglSurface() const { return mEglSurface; }
    UbuntuInput *input() { return mInput; }

private:
    void createWindow();
    void moveResize(const QRect& rect);

    UbuntuScreen* mScreen;
    EGLSurface mEglSurface;
    WId mId;
    UbuntuInput* mInput;
    UAUiWindow* mWindow;
    Qt::WindowState mState;
    QRect mGeometry;
    UApplicationInstance* mUainstance;
    UAUiWindowProperties* mWprops;
};

#endif // UBUNTU_WINDOW_H
