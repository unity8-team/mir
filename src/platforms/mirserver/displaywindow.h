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
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#ifndef DISPLAYWINDOW_H
#define DISPLAYWINDOW_H

#include <qpa/qplatformwindow.h>

#include <mirplatform/mir/graphics/display_buffer.h>

// DisplayWindow wraps the whatever implementation Mir creates of a DisplayBuffer,
// which is the buffer output for an individual display.

class DisplayWindow : public QPlatformWindow
{
public:
    explicit DisplayWindow(QWindow *window, mir::graphics::DisplayBuffer*);

    QRect geometry() const override;
    void setGeometry(const QRect &rect) override;
    WId winId() const { return m_winId; }

    void swapBuffers();
    void makeCurrent();
    void doneCurrent();

private:
    WId m_winId;
    mir::graphics::DisplayBuffer *m_displayBuffer;
};

#endif // DISPLAYWINDOW_H
