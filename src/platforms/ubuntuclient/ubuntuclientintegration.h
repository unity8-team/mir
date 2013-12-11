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

#ifndef UBUNTU_CLIENT_INTEGRATION_H
#define UBUNTU_CLIENT_INTEGRATION_H

#include <qpa/qplatformintegration.h>

#include "ubuntuplatformservices.h"

// platform-api
#include <ubuntu/application/description.h>
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/options.h>
#include <ubuntu/application/ui/session.h>

class UbuntuInput;
class UbuntuScreen;

class UbuntuClientIntegration : public QPlatformIntegration {
public:
    UbuntuClientIntegration();
    virtual ~UbuntuClientIntegration();

    // QPlatformIntegration methods.
    bool hasCapability(QPlatformIntegration::Capability cap) const override;
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QAbstractEventDispatcher *guiThreadEventDispatcher() const override { return mEventDispatcher; }
#else
    QAbstractEventDispatcher *createEventDispatcher() const override;
#endif
    QPlatformNativeInterface* nativeInterface() const override { return mNativeInterface; }
    QPlatformBackingStore* createPlatformBackingStore(QWindow* window) const override;
    QPlatformOpenGLContext* createPlatformOpenGLContext(QOpenGLContext* context) const override;
    QPlatformFontDatabase* fontDatabase() const override { return mFontDb; }
    QStringList themeNames() const override;
    QPlatformTheme* createPlatformTheme(const QString& name) const override;
    QPlatformServices *services() const override;
    QPlatformWindow* createPlatformWindow(QWindow* window) const override;
    QPlatformInputContext* inputContext() const override { return mInputContext; }
    QPlatformClipboard* clipboard() const override { return mClipboard; }

    QPlatformOpenGLContext* createPlatformOpenGLContext(QOpenGLContext* context);
    QPlatformWindow* createPlatformWindow(QWindow* window);
    UbuntuScreen* screen() const { return mScreen; }

private:
    void setupOptions();
    void setupDescription();

    QPlatformNativeInterface* mNativeInterface;
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QAbstractEventDispatcher* mEventDispatcher;
#endif
    QPlatformFontDatabase* mFontDb;

    UbuntuPlatformServices* mServices;

    UbuntuScreen* mScreen;
    UbuntuInput* mInput;
    QPlatformInputContext* mInputContext;
    QPlatformClipboard* mClipboard;

    // Platform API stuff
    UApplicationOptions* mOptions;
    UApplicationDescription* mDesc;
    UApplicationInstance* mInstance;
    UAUiSessionProperties* mProps;
    UAUiSession* mSession;
};

#endif // UBUNTU_CLIENT_INTEGRATION_H
