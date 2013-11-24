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

#ifndef MIRSERVERINTEGRATION_H
#define MIRSERVERINTEGRATION_H

// qt
#include <qpa/qplatformintegration.h>

// local
#include "qmirserver.h"
#include "mirserverconfiguration.h"
#include "display.h"
#include "nativeinterface.h"

class MirServerIntegration : public QPlatformIntegration
{
public:
    MirServerIntegration();
    ~MirServerIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;

#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QAbstractEventDispatcher* guiThreadEventDispatcher() const override { return eventDispatcher_; }
    void initialize();
#else
    QAbstractEventDispatcher *createEventDispatcher() const override;
    void initialize() override;
#endif

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;

    QPlatformAccessibility *accessibility() const override;

    QPlatformNativeInterface *nativeInterface() const override;

private:
    QScopedPointer<QPlatformAccessibility> m_accessibility;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
    QMirServer *m_mirServer;
    Display *m_display;
    MirServerConfiguration *m_mirConfig;
    NativeInterface *m_nativeInterface;
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QAbstractEventDispatcher* eventDispatcher_;
#endif
};

#endif // MIRSERVERINTEGRATION_H
