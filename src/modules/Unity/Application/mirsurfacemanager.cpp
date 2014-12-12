/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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
#include <QMutexLocker>

// local
#include "mirsurfacemanager.h"
#include "sessionmanager.h"
#include "application_manager.h"
#include "tracepoints.h" // generated from tracepoints.tp

// common
#include <debughelpers.h>

// QPA mirserver
#include "nativeinterface.h"
#include "mirserver.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "logging.h"

Q_LOGGING_CATEGORY(QTMIR_SURFACES, "qtmir.surfaces")

namespace ms = mir::scene;

namespace qtmir {

MirSurfaceManager *MirSurfaceManager::the_surface_manager = nullptr;


void connectToSessionListener(MirSurfaceManager *manager, SessionListener *listener)
{
    QObject::connect(listener, &SessionListener::sessionCreatedSurface,
                     manager, &MirSurfaceManager::onSessionCreatedSurface);
    QObject::connect(listener, &SessionListener::sessionDestroyingSurface,
                     manager, &MirSurfaceManager::onSessionDestroyingSurface);
}

void connectToSurfaceConfigurator(MirSurfaceManager *manager, SurfaceConfigurator *surfaceConfigurator)
{
    QObject::connect(surfaceConfigurator, &SurfaceConfigurator::surfaceAttributeChanged,
                     manager, &MirSurfaceManager::onSurfaceAttributeChanged);
}

MirSurfaceManager* MirSurfaceManager::singleton()
{
    if (!the_surface_manager) {

        NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

        if (!nativeInterface) {
            qCritical("ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin");
            QGuiApplication::quit();
            return nullptr;
        }

        SessionListener *sessionListener = static_cast<SessionListener*>(nativeInterface->nativeResourceForIntegration("SessionListener"));
        SurfaceConfigurator *surfaceConfigurator = static_cast<SurfaceConfigurator*>(nativeInterface->nativeResourceForIntegration("SessionConfigurator"));

        the_surface_manager = new MirSurfaceManager(nativeInterface->m_mirServer, SessionManager::singleton());

        connectToSessionListener(the_surface_manager, sessionListener);
        connectToSurfaceConfigurator(the_surface_manager, surfaceConfigurator);
    }
    return the_surface_manager;
}

MirSurfaceManager::MirSurfaceManager(
        const QSharedPointer<MirServer>& mirServer,
        SessionManager* sessionManager,
        QObject *parent)
    : MirSurfaceItemModel(parent)
    , m_mirServer(mirServer)
    , m_sessionManager(sessionManager)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::MirSurfaceManager - this=" << this;
    setObjectName("qtmir::SurfaceManager");
}

MirSurfaceManager::~MirSurfaceManager()
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::~MirSurfaceManager - this=" << this;

    m_mirSurfaceToItemHash.clear();
}

void MirSurfaceManager::onSessionCreatedSurface(const mir::scene::Session *mirSession,
                                                const std::shared_ptr<mir::scene::Surface> &surface,
                                                const std::shared_ptr<mir::scene::SurfaceObserver> &observer)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSessionCreatedSurface - mirSession=" << mirSession
                            << "surface=" << surface.get() << "surface.name=" << surface->name().c_str();

    SessionInterface* session = m_sessionManager->findSession(mirSession);
    auto qmlSurface = new MirSurfaceItem(surface, session, observer);
    {
        QMutexLocker lock(&m_mutex);
        m_mirSurfaceToItemHash.insert(surface.get(), qmlSurface);
    }

    if (session)
        session->setSurface(qmlSurface);

    // Only notify QML of surface creation once it has drawn its first frame.
    connect(qmlSurface, &MirSurfaceItem::firstFrameDrawn, this, [&](MirSurfaceItem *item) {
        tracepoint(qtmir, firstFrameDrawn);
        Q_EMIT surfaceCreated(item);

        insert(0, item);
    });

    // clean up after MirSurfaceItem is destroyed
    connect(qmlSurface, &MirSurfaceItem::destroyed, this, [&](QObject *item) {
        auto mirSurfaceItem = static_cast<MirSurfaceItem*>(item);
        {
            QMutexLocker lock(&m_mutex);
            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(mirSurfaceItem));
        }

        remove(mirSurfaceItem);
        tracepoint(qtmir, surfaceDestroyed);
    });
    tracepoint(qtmir, surfaceCreated);
}

void MirSurfaceManager::onSessionDestroyingSurface(const mir::scene::Session *session,
                                                   const std::shared_ptr<mir::scene::Surface> &surface)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSessionDestroyingSurface - session=" << session
                            << "surface=" << surface.get() << "surface.name=" << surface->name().c_str();

    MirSurfaceItem* item = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_mirSurfaceToItemHash.find(surface.get());
        if (it != m_mirSurfaceToItemHash.end()) {

            item = it.value();

            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(item));
        } else {
            qCritical() << "MirSurfaceManager::onSessionDestroyingSurface: unable to find MirSurfaceItem corresponding"
                        << "to surface=" << surface.get() << "surface.name=" << surface->name().c_str();
            return;
        }
    }

    item->setEnabled(false); //disable input events
    item->setLive(false); //disable input events
    Q_EMIT surfaceDestroyed(item);
}

// NB: Surface might be a dangling pointer here, so refrain from dereferencing it.
void MirSurfaceManager::onSurfaceAttributeChanged(const ms::Surface *surface,
                                                  const MirSurfaceAttrib attribute, const int value)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSurfaceAttributeChanged - surface=" << surface
                            << qPrintable(mirSurfaceAttribAndValueToString(attribute, value));

    QMutexLocker lock(&m_mutex);
    auto it = m_mirSurfaceToItemHash.find(surface);
    if (it != m_mirSurfaceToItemHash.end()) {
        it.value()->setAttribute(attribute, value);
    }
}


} // namespace qtmir
