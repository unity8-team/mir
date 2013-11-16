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
#include "mirsurface.h"
#include "application_manager.h"

// unity-mir
#include "nativeinterface.h"
#include "mirserverconfiguration.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "logging.h"

namespace msh = mir::shell;

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
    , m_shellSurface(nullptr)
{
    DLOG("MirSurfaceManager::MirSurfaceManager (this=%p)", this);

    NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

    if (!nativeInterface) {
        LOG("ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin");
        QGuiApplication::quit();
        return;
    }

    SessionListener *sessionListener = static_cast<SessionListener*>(nativeInterface->nativeResourceForIntegration("sessionlistener"));
    SurfaceConfigurator *surfaceConfigurator = static_cast<SurfaceConfigurator*>(nativeInterface->nativeResourceForIntegration("surfaceconfigurator"));

//    QObject::connect(m_mirServer->surfaceFactory(), &SurfaceFactory::shellSurfaceCreated,
//                     this, &MirSurfaceManager::shellSurfaceCreated);

    QObject::connect(sessionListener, &SessionListener::sessionCreatedSurface,
                     this, &MirSurfaceManager::sessionCreatedSurface);
    QObject::connect(sessionListener, &SessionListener::sessionDestroyingSurface,
                     this, &MirSurfaceManager::sessionDestroyingSurface);

    QObject::connect(surfaceConfigurator, &SurfaceConfigurator::surfaceAttributeChanged,
                     this, &MirSurfaceManager::surfaceAttributeChanged);
}

MirSurfaceManager::~MirSurfaceManager()
{
    DLOG("MirSurfaceManager::~MirSurfaceManager (this=%p)", this);

    Q_FOREACH(auto surface, m_surfaces) {
        delete surface;
    }
    m_surfaces.clear();
    delete m_shellSurface;
}

MirSurface *MirSurfaceManager::shellSurface() const
{
    return m_shellSurface;
}

MirSurface *MirSurfaceManager::surfaceFor(std::shared_ptr<mir::shell::Surface> const& surface)
{
    auto it = m_surfaces.find(surface.get());
    if (it != m_surfaces.end()) {
        return *it;
    } else {
        DLOG("MirSurfaceManager::surfaceFor (this=%p) with surface name '%s' asking for a surface that was not created", this, surface->name().c_str());
        return nullptr;
    }
}

void MirSurfaceManager::sessionCreatedSurface(mir::shell::ApplicationSession const* session, std::shared_ptr<mir::shell::Surface> const& surface)
{
    DLOG("MirSurfaceManager::sessionCreatedSurface (this=%p) with surface name '%s'", this, surface->name().c_str());
    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());
    Application* application = appMgr->findApplicationWithSession(session);
    
    auto qmlSurface = new MirSurface(surface, application);
    m_surfaces.insert(surface.get(), qmlSurface);
    Q_EMIT surfaceCreated(qmlSurface);
}

void MirSurfaceManager::sessionDestroyingSurface(mir::shell::ApplicationSession const*, std::shared_ptr<mir::shell::Surface> const& surface)
{
    DLOG("MirSurfaceManager::sessionDestroyingSurface (this=%p) with surface name '%s'", this, surface->name().c_str());

    auto it = m_surfaces.find(surface.get());
    if (it != m_surfaces.end()) {
        Q_EMIT surfaceDestroyed(*it);
        delete *it;
        m_surfaces.erase(it);
        return;
    }

    DLOG("MirSurfaceManager::sessionDestroyingSurface: unable to find MirSurface corresponding to surface '%s'", surface->name().c_str());
}

void MirSurfaceManager::shellSurfaceCreated(const std::shared_ptr<msh::Surface> &surface)
{
    DLOG("MirSurfaceManager::shellSurfaceCreated (this=%p)", this);
    m_shellSurface = new MirSurface(surface, nullptr);

//    FocusSetter *fs = m_mirServer->focusSetter();
//    if (fs) {
//        fs->set_default_keyboard_target(surface);
//    }
    
    Q_EMIT shellSurfaceChanged(m_shellSurface);
}

void MirSurfaceManager::surfaceAttributeChanged(const msh::Surface *surface, const MirSurfaceAttrib attribute, const int value)
{
    DLOG("MirSurfaceManager::surfaceAttributeChanged (this=%p, attrib=%d, value=%d)",
         this, static_cast<int>(attribute), value);

    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        it.value()->setAttribute(attribute, value);
        if (attribute == mir_surface_attrib_state &&
                value == mir_surface_state_fullscreen) {
            it.value()->application()->setFullscreen(static_cast<bool>(value));
        }
    }
}
