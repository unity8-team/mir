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
    : QAbstractListModel(parent)
{
    DLOG("MirSurfaceManager::MirSurfaceManager (this=%p)", this);

    m_roleNames.insert(RoleSurface, "surface");

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

    m_mirSurfaceToItemHash.clear();
}

void MirSurfaceManager::onSessionCreatedSurface(mir::scene::Session const* session,
        std::shared_ptr<mir::scene::Surface> const& surface)
{
    DLOG("MirSurfaceManager::onSessionCreatedSurface (this=%p) with surface name '%s'", this, surface->name().c_str());
    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());

    auto qmlSurface = new MirSurfaceItem(surface);
    m_mirSurfaceToItemHash.insert(surface.get(), qmlSurface);

    Application* application = appMgr->findApplicationWithSession(session);
    application->setSurface(qmlSurface);

    bool wasEmpty = isEmpty();
    beginInsertRows(QModelIndex(), 0, 0);
    m_surfaceItems.prepend(qmlSurface);
    endInsertRows();
    emit countChanged();
    if (wasEmpty && !isEmpty()) {
        emit emptyChanged();
    }
    emit topmostSurfaceChanged();

    // Only notify QML of surface creation once it has drawn its first frame.
    connect(qmlSurface, &MirSurfaceItem::firstFrameDrawn, [&](MirSurfaceItem *item) {
        Q_EMIT surfaceCreated(item);
    });

    // clean up after MirSurfaceItem is destroyed
    connect(qmlSurface, &MirSurfaceItem::destroyed, [&](QObject *item) {
        auto mirSurfaceItem = static_cast<MirSurfaceItem*>(item);
        m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(mirSurfaceItem));

        int i = m_surfaceItems.indexOf(mirSurfaceItem);
        if (i != -1) {
            bool wasEmpty = isEmpty();
            beginRemoveRows(QModelIndex(), i, i);
            m_surfaceItems.removeAt(i);
            endRemoveRows();
            emit countChanged();
            if (!wasEmpty && isEmpty()) {
                emit emptyChanged();
            }
            if (i == 0) {
                emit topmostSurfaceChanged();
            }
        }
    });
}

void MirSurfaceManager::onSessionDestroyingSurface(mir::scene::Session const*,
        std::shared_ptr<mir::scene::Surface> const& surface)
{
    DLOG("MirSurfaceManager::onSessionDestroyingSurface (this=%p) with surface name '%s'", this, surface->name().c_str());

    auto it = m_mirSurfaceToItemHash.find(surface.get());
    if (it != m_mirSurfaceToItemHash.end()) {
        Q_EMIT surfaceDestroyed(*it);
        MirSurfaceItem* item = it.value();
        Q_EMIT item->surfaceDestroyed();

        m_mirSurfaceToItemHash.erase(it);

        int i = m_surfaceItems.indexOf(item);
        if (i != -1) {
            bool wasEmpty = isEmpty();
            beginRemoveRows(QModelIndex(), i, i);
            m_surfaceItems.removeAt(i);
            endRemoveRows();
            emit countChanged();
            if (!wasEmpty && isEmpty()) {
                emit emptyChanged();
            }
            if (i == 0) {
                emit topmostSurfaceChanged();
            }
        }
        return;
    }

    DLOG("MirSurfaceManager::sessionDestroyingSurface: unable to find MirSurfaceItem corresponding to surface '%s'", surface->name().c_str());
}

void MirSurfaceManager::onSurfaceAttributeChanged(const ms::Surface *surface,
        const MirSurfaceAttrib attribute, const int value)
{
    DLOG("MirSurfaceManager::onSurfaceAttributeChanged (surface='%s', attrib=%d, value=%d)",
         surface->name().c_str(), static_cast<int>(attribute), value);

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

void MirSurfaceManager::move(int from, int to) {
    DLOG("MirSurfaceManager::move (this=%p, from=%d, to=%d)", this, from, to);
    if (from == to) return;

    if (from >= 0 && from < m_surfaceItems.count() && to >= 0 && to < m_surfaceItems.count()) {
        QModelIndex parent;
        void *oldTopMost = topmostSurface();

        /* When moving an item down, the destination index needs to be incremented
           by one, as explained in the documentation:
           http://qt-project.org/doc/qt-5.0/qtcore/qabstractitemmodel.html#beginMoveRows */
        beginMoveRows(parent, from, from, parent, to + (to > from ? 1 : 0));
        m_surfaceItems.move(from, to);
        endMoveRows();

        if (oldTopMost != topmostSurface()) {
            emit topmostSurfaceChanged();
        }
    }
}

MirSurfaceItem* MirSurfaceManager::getSurface(int index)
{
    return m_surfaceItems[index];
}

MirSurfaceItem* MirSurfaceManager::topmostSurface() const
{
    if (m_surfaceItems.isEmpty()) {
        return nullptr;
    } else {
        return m_surfaceItems[0];
    }
}
