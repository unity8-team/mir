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

// local
#include "mirserverconfiguration.h"

// qt
#include <qpa/qplatformintegration.h>

class Display;
class NativeInterface;
class QMirServer;

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

    QPlatformInputContext* inputContext() const override { return m_inputContext; }

    QPlatformFontDatabase *fontDatabase() const override;
    QStringList themeNames() const override;
    QPlatformTheme* createPlatformTheme(const QString& name) const override;
    QPlatformServices *services() const override;

    QPlatformAccessibility *accessibility() const override;

    QPlatformNativeInterface *nativeInterface() const override;

private:
    QSharedPointer<MirServerConfiguration> m_mirConfig;

    QScopedPointer<QPlatformAccessibility> m_accessibility;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QScopedPointer<QAbstractEventDispatcher> m_eventDispatcher;
#endif

    Display *m_display;
    QMirServer *m_mirServer;
    NativeInterface *m_nativeInterface;
    QPlatformInputContext* m_inputContext;
};

#endif // MIRSERVERINTEGRATION_H
