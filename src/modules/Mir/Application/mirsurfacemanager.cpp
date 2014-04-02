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

// Qt
#include <QGuiApplication>

// local
#include "mirsurfacemanager.h"
#include "application_manager.h"

// unity-mir
#include "nativeinterface.h"
#include "mirserverconfiguration.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "logging.h"

namespace msh = mir::shell;
namespace ms = mir::scene;

MirSurfaceManager *MirSurfaceManager::the_surface_manager = nullptr;

MirSurfaceManager* MirSurfaceManager::singleton()
{
    if (!the_surface_manager) {
        the_surface_manager = new MirSurfaceManager();
    }
    return the_surface_manager;
}

MirSurfaceManager::MirSurfaceManager(QObject *parent)
    : QObject(parent)
{
    DLOG("MirSurfaceManager::MirSurfaceManager (this=%p)", this);

    NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

    if (!nativeInterface) {
        LOG("ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin");
        QGuiApplication::quit();
        return;
    }

    SessionListener *sessionListener = static_cast<SessionListener*>(nativeInterface->nativeResourceForIntegration("SessionListener"));
    SurfaceConfigurator *surfaceConfigurator = static_cast<SurfaceConfigurator*>(nativeInterface->nativeResourceForIntegration("SessionConfigurator"));

    QObject::connect(sessionListener, &SessionListener::sessionCreatedSurface,
                     this, &MirSurfaceManager::onSessionCreatedSurface);
    QObject::connect(sessionListener, &SessionListener::sessionDestroyingSurface,
                     this, &MirSurfaceManager::onSessionDestroyingSurface);

    QObject::connect(surfaceConfigurator, &SurfaceConfigurator::surfaceAttributeChanged,
                     this, &MirSurfaceManager::onSurfaceAttributeChanged);
}

MirSurfaceManager::~MirSurfaceManager()
{
    DLOG("MirSurfaceManager::~MirSurfaceManager (this=%p)", this);

    m_surfaces.clear();
}

void MirSurfaceManager::onSessionCreatedSurface(mir::shell::Session const* session,
        std::shared_ptr<mir::shell::Surface> const& surface)
{
    DLOG("MirSurfaceManager::onSessionCreatedSurface (this=%p) with surface name '%s'", this, surface->name().c_str());
    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());
    Application* application = appMgr->findApplicationWithSession(session);

    std::shared_ptr<ms::Surface> const& sceneSurface = std::static_pointer_cast<ms::Surface>(surface);

    auto qmlSurface = new MirSurfaceItem(sceneSurface, application);
    m_surfaces.insert(sceneSurface.get(), qmlSurface);

    // Only notify QML of surface creation once it has drawn its first frame.
    connect(qmlSurface, &MirSurfaceItem::surfaceFirstFrameDrawn, [&](MirSurfaceItem *item) {
        Q_EMIT surfaceCreated(item);
    });

    // clean up after MirSurfaceItem is destroyed
    connect(qmlSurface, &MirSurfaceItem::destroyed, [&](QObject *item) {
        auto mirSurfaceItem = static_cast<MirSurfaceItem*>(item);
        m_surfaces.remove(m_surfaces.key(mirSurfaceItem));
    });
}

void MirSurfaceManager::onSessionDestroyingSurface(mir::shell::Session const*,
        std::shared_ptr<mir::shell::Surface> const& surface)
{
    DLOG("MirSurfaceManager::onSessionDestroyingSurface (this=%p) with surface name '%s'", this, surface->name().c_str());

    std::shared_ptr<ms::Surface> const& sceneSurface = std::static_pointer_cast<ms::Surface>(surface);

    auto it = m_surfaces.find(sceneSurface.get());
    if (it != m_surfaces.end()) {
        Q_EMIT surfaceDestroyed(*it);
        MirSurfaceItem* item = it.value();
        Q_EMIT item->surfaceDestroyed();
        // delete *it; // do not delete actual MirSurfaceItem as QML has ownership of that object.
        m_surfaces.erase(it);
        return;
    }

    DLOG("MirSurfaceManager::sessionDestroyingSurface: unable to find MirSurfaceItem corresponding to surface '%s'", surface->name().c_str());
}

void MirSurfaceManager::onSurfaceAttributeChanged(const ms::Surface *surface,
        const MirSurfaceAttrib attribute, const int value)
{
    DLOG("MirSurfaceManager::onSurfaceAttributeChanged (surface='%s', attrib=%d, value=%d)",
         surface->name().c_str(), static_cast<int>(attribute), value);

    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        it.value()->setAttribute(attribute, value);
        if (attribute == mir_surface_attrib_state &&
                value == mir_surface_state_fullscreen) {
            it.value()->application()->setFullscreen(static_cast<bool>(value));
        }
    }
}
