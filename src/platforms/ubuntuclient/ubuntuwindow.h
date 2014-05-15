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

class UbuntuInput;
class UbuntuScreen;
class UbuntuWindowPrivate;

class UbuntuWindow : public QPlatformWindow
{
public:
    UbuntuWindow(QWindow* w, UbuntuScreen* screen,
                 UbuntuInput* input, void* instance);
    virtual ~UbuntuWindow();

    // QPlatformWindow methods.
    WId winId() const override;
    void setGeometry(const QRect&) override;
    void setWindowState(Qt::WindowState state) override;
    void setVisible(bool visible) override;

    // New methods.
    void* eglSurface() const;
    void handleResize(int width, int height);

    UbuntuWindowPrivate* priv() { return d; }
private:
    void createWindow();
    void moveResize(const QRect& rect);

    UbuntuWindowPrivate *d;
};

#endif // UBUNTU_WINDOW_H
