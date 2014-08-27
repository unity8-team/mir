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
#include "application_manager.h"

// common
#include <debughelpers.h>

// QPA mirserver
#include "nativeinterface.h"
#include "mirserverconfiguration.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "logging.h"
#include "promptsessionlistener.h"

// mir
#include <mir/scene/prompt_session.h>
#include <mir/scene/prompt_session_manager.h>


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

void connectToPromptSessionListener(MirSurfaceManager * manager, PromptSessionListener * listener)
{
    QObject::connect(listener, &PromptSessionListener::promptProviderAdded,
                     manager, &MirSurfaceManager::onPromptProviderAdded);
    QObject::connect(listener, &PromptSessionListener::promptProviderRemoved,
                     manager, &MirSurfaceManager::onPromptProviderRemoved);
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
        PromptSessionListener *promptSessionListener = static_cast<PromptSessionListener*>(nativeInterface->nativeResourceForIntegration("PromptSessionListener"));

        the_surface_manager = new MirSurfaceManager(nativeInterface->m_mirConfig);

        connectToSessionListener(the_surface_manager, sessionListener);
        connectToSurfaceConfigurator(the_surface_manager, surfaceConfigurator);
        connectToPromptSessionListener(the_surface_manager, promptSessionListener);
    }
    return the_surface_manager;
}

MirSurfaceManager::MirSurfaceManager(
        const QSharedPointer<MirServerConfiguration>& mirConfig,
        QObject *parent)
    : QAbstractListModel(parent)
    , m_mirConfig(mirConfig)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::MirSurfaceManager - this=" << this;
    setObjectName("qtmir::SurfaceManager");
}

MirSurfaceManager::~MirSurfaceManager()
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::~MirSurfaceManager - this=" << this;

    m_mirSurfaceToItemHash.clear();
}

QHash<int, QByteArray> MirSurfaceManager::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames.insert(RoleSurface, "surface");
    return roleNames;
}

void MirSurfaceManager::onSessionCreatedSurface(const mir::scene::Session *session,
                                                const std::shared_ptr<mir::scene::Surface> &surface)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onSessionCreatedSurface - session=" << session
                            << "surface=" << surface.get() << "surface.name=" << surface->name().c_str();

    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());

    Application* application = appMgr->findApplicationWithSession(session, false);
    auto qmlSurface = new MirSurfaceItem(surface, application);
    {
        QMutexLocker lock(&m_mutex);
        m_mirSurfaceToItemHash.insert(surface.get(), qmlSurface);
        m_mirSessionToItemHash.insert(session, qmlSurface);
    }

    if (application)
        application->setSurface(qmlSurface);

    // Only notify QML of surface creation once it has drawn its first frame.
    connect(qmlSurface, &MirSurfaceItem::firstFrameDrawn, [&](MirSurfaceItem *item) {
        Q_EMIT surfaceCreated(item);

        beginInsertRows(QModelIndex(), 0, 0);
        m_surfaceItems.prepend(item);
        endInsertRows();
        Q_EMIT countChanged();

        refreshPromptSessionSurfaces(m_mirSessionToItemHash.key(item));
    });

    // clean up after MirSurfaceItem is destroyed
    connect(qmlSurface, &MirSurfaceItem::destroyed, [&](QObject *item) {
        auto mirSurfaceItem = static_cast<MirSurfaceItem*>(item);
        {
            QMutexLocker lock(&m_mutex);
            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(mirSurfaceItem));
            m_mirSessionToItemHash.remove(m_mirSessionToItemHash.key(mirSurfaceItem));
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

    MirSurfaceItem* item = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_mirSurfaceToItemHash.find(surface.get());
        if (it != m_mirSurfaceToItemHash.end()) {

            item = it.value();

            m_mirSurfaceToItemHash.remove(m_mirSurfaceToItemHash.key(item));
            m_mirSessionToItemHash.remove(m_mirSessionToItemHash.key(item));
        } else {
            qCritical() << "MirSurfaceManager::onSessionDestroyingSurface: unable to find MirSurfaceItem corresponding"
                        << "to surface=" << surface.get() << "surface.name=" << surface->name().c_str();
            return;
        }
    }

    Q_EMIT surfaceDestroyed(item);
    item->setEnabled(false); //disable input events
    Q_EMIT item->removed();
    Q_EMIT item->surfaceDestroyed();
}

void MirSurfaceManager::onPromptProviderAdded(const mir::scene::PromptSession *promptSession,
                                              const std::shared_ptr<mir::scene::Session> &session)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onPromptProviderAdded - promptSession=" << promptSession
                            << "session=" << session.get();

    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());
    Application* application = appMgr->findApplicationWithPromptSession(promptSession);

    refreshPromptSessionSurfaces(application);
}

void MirSurfaceManager::onPromptProviderRemoved(const mir::scene::PromptSession *promptSession,
                                                const std::shared_ptr<mir::scene::Session> &session)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::onPromptProviderRemoved - promptSession=" << promptSession
                            << "session=" << session.get();

    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());
    Application* application = appMgr->findApplicationWithPromptSession(promptSession);

    refreshPromptSessionSurfaces(application);
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

void appendSurfaceDecendents(MirSurfaceItem* item, QList<MirSurfaceItem*>& surfaceChildren)
{
    // recursive function. fetch all decendent surface items as a list.
    item->foreachChildSurface([&surfaceChildren](MirSurfaceItem* child) {
        surfaceChildren.append(child);
        appendSurfaceDecendents(child, surfaceChildren);
    });
}

void MirSurfaceManager::refreshPromptSessionSurfaces(const mir::scene::Session* session)
{
    if (!session)
        return;
    ApplicationManager* appMgr = static_cast<ApplicationManager*>(ApplicationManager::singleton());
    Application* application = appMgr->findApplicationWithSession(session, true);

    refreshPromptSessionSurfaces(application);
}

void MirSurfaceManager::refreshPromptSessionSurfaces(Application* application)
{
    // Re-contruct the child surface heirachy from prompt session providers.
    // First provider becomes prompt surface of Application, subsequent providers
    // becoming the children of the previous provider.

    if (!application) {
        qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::refreshPromptSessionSurfaces - No Application for prompt session";
        return;
    }
    qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::refreshPromptSessionSurfaces - appId=" << application->name();

    std::shared_ptr<ms::PromptSessionManager> manager = m_mirConfig->the_prompt_session_manager();

    QList<MirSurfaceItem*> surfaces;
    application->foreachPromptSurface([&surfaces](MirSurfaceItem* promptSurface) {
        surfaces.append(promptSurface);
        appendSurfaceDecendents(promptSurface, surfaces);
    });
    // stop adding providers to the child stack if we come across one which has not yet added a surface.
    bool continueProviders = true;

    auto refreshFn = [&](const std::shared_ptr<mir::scene::PromptSession>& promptSession) {

        MirSurfaceItem* parentSurface = nullptr;

        manager->for_each_provider_in(promptSession,
            [&](const std::shared_ptr<ms::Session> &session) {
                QMutexLocker lock(&m_mutex);

                if (!continueProviders)
                    return;

                auto it = m_mirSessionToItemHash.find(session.get());
                MirSurfaceItem *nextParentSurface = nullptr;

                while (it != m_mirSessionToItemHash.end() && it.key() == session.get()) { // all surfaces for session
                    MirSurfaceItem *surface = it.value();
                    surfaces.removeOne(surface);

                    if (!surface->isFirstFrameDrawn()) { // only add if the surface is ready.
                        continueProviders = false;
                        break;
                    }

                    // FIXME - all providers should be "prompt surfaces" of application

                    // first privder is a application "prompt surface"
                    if (parentSurface == nullptr) {
                        surface->setParentSurface(nullptr);
                        application->addPromptSurface(surface);
                    // susquent provider is child of previous provider
                    } else {
                        surface->setParentSurface(parentSurface);
                    }
                    if (!nextParentSurface)
                        nextParentSurface = surface;
                    ++it;
                }
                // first valid session surface is parent of next provider session surface.
                if (nextParentSurface) {
                    parentSurface = nextParentSurface;
                }
            });

        for (MirSurfaceItem* item : surfaces) {
            qCDebug(QTMIR_SURFACES) << "MirSurfaceManager::refreshPromptSessionSurfaces - remove: " << item->name();
            Q_EMIT item->removed();
        }
    };

    application->foreachPromptSession(refreshFn);
}

} // namespace qtmir
