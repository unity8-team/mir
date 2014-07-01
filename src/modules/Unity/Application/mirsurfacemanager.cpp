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
#include "debughelpers.h"
#include "mirsurfacemanager.h"
#include "application_manager.h"

// QPA mirserver
#include "nativeinterface.h"
#include "mirserverconfiguration.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "logging.h"

Q_LOGGING_CATEGORY(QTMIR_SURFACES, "qtmir.surfaces")

namespace ms = mir::scene;

namespace qtmir {

MirSurfaceManager *MirSurfaceManager::the_surface_manager = nullptr;

MirSurfaceManager* MirSurfaceManager::singleton()
{
    if (!the_surface_manager) {
        the_surface_manager = new MirSurfaceManager();
    }
    return the_surface_manager;
}

MirSurfaceManager::MirSurfaceManager(QObject *parent)
    : QAbstractListModel(parent)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::MirSurfaceManager - this=" << this;

    m_roleNames.insert(RoleSurface, "surface");

    NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

    if (!nativeInterface) {
        qCritical("ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin");
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
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::~MirSurfaceManager - this=" << this;

    m_mirSurfaceToItemHash.clear();
}

void MirSurfaceManager::onSessionCreatedSurface(const mir::scene::Session *session,
                                                const std::shared_ptr<mir::scene::Surface> &surface)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSessionCreatedSurface - session=" << session
                            << "surface=" << surface.get() << "surface.name=" << surface->name().c_str();

    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());

    Application* application = appMgr->findApplicationWithSession(session);
    auto qmlSurface = new MirSurfaceItem(surface, application);
    {
        QMutexLocker lock(&m_mutex);
        m_mirSurfaceToItemHash.insert(surface.get(), qmlSurface);
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_surfaceItems.prepend(qmlSurface);
    endInsertRows();
    Q_EMIT countChanged();

    if (application)
        application->setSurface(qmlSurface);

    // Only notify QML of surface creation once it has drawn its first frame.
    connect(qmlSurface, &MirSurfaceItem::firstFrameDrawn, [&](MirSurfaceItem *item) {
        Q_EMIT surfaceCreated(item);

        QMutexLocker lock(&m_mutex);
        beginInsertRows(QModelIndex(), 0, 0);
        m_surfaceItems.prepend(item);
        endInsertRows();
        Q_EMIT countChanged();
    });

    // clean up after MirSurfaceItem is destroyed
    connect(qmlSurface, &MirSurfaceItem::destroyed, [&](QObject *item) {
        auto mirSurfaceItem = static_cast<MirSurfaceItem*>(item);
        {
            QMutexLocker lock(&m_mutex);
            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(mirSurfaceItem));
        }

        int i = m_surfaceItems.indexOf(mirSurfaceItem);
        if (i != -1) {
            beginRemoveRows(QModelIndex(), i, i);
            m_surfaceItems.removeAt(i);
            endRemoveRows();
            Q_EMIT countChanged();
        }
    });
}

void MirSurfaceManager::onSessionDestroyingSurface(const mir::scene::Session *session,
                                                   const std::shared_ptr<mir::scene::Surface> &surface)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSessionDestroyingSurface - session=" << session
                            << "surface=" << surface.get() << "surface.name=" << surface->name().c_str();

    auto it = m_mirSurfaceToItemHash.find(surface.get());
    if (it != m_mirSurfaceToItemHash.end()) {
        Q_EMIT surfaceDestroyed(*it);
        MirSurfaceItem* item = it.value();
        Q_EMIT item->surfaceDestroyed();

        {
            QMutexLocker lock(&m_mutex);
            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(item));
        }
//        m_mirSurfaceToItemHash.erase(it);

        int i = m_surfaceItems.indexOf(item);
        if (i != -1) {
            beginRemoveRows(QModelIndex(), i, i);
            m_surfaceItems.removeAt(i);
            endRemoveRows();
            Q_EMIT countChanged();
        }
        return;
    }

    qCritical() << "MirSurfaceManager::onSessionDestroyingSurface: unable to find MirSurfaceItem corresponding"
                << "to surface=" << surface.get() << "surface.name=" << surface->name().c_str();
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

int MirSurfaceManager::rowCount(const QModelIndex & /*parent*/) const
{
    return m_surfaceItems.count();
}

QVariant MirSurfaceManager::data(const QModelIndex & index, int role) const
{
    if (index.row() >= 0 && index.row() < m_surfaceItems.count()) {
        MirSurfaceItem *surfaceItem = m_surfaceItems.at(index.row());
        switch (role) {
            case RoleSurface:
                return QVariant::fromValue(surfaceItem);
            default:
                return QVariant();
        }
    } else {
        return QVariant();
    }
}

MirSurfaceItem* MirSurfaceManager::getSurface(int index)
{
    return m_surfaceItems[index];
}

} // namespace qtmir
