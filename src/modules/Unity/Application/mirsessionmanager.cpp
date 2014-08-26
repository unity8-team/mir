/*
 * Copyright (C) 2014 Canonical, Ltd.
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
#include "application_manager.h"
#include "debughelpers.h"
#include "mirsessionmanager.h"

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

Q_LOGGING_CATEGORY(QTMIR_SESSIONS, "qtmir.sessions")

namespace ms = mir::scene;

namespace qtmir {

MirSessionManager *MirSessionManager::the_session_manager = nullptr;


void connectToSessionListener(MirSessionManager *manager, SessionListener *listener)
{
    QObject::connect(listener, &SessionListener::sessionStarting,
                     manager, &MirSessionManager::onSessionStarting);
    QObject::connect(listener, &SessionListener::sessionStopping,
                     manager, &MirSessionManager::onSessionStopping);
}

void connectToPromptSessionListener(MirSessionManager * manager, PromptSessionListener * listener)
{
    QObject::connect(listener, &PromptSessionListener::promptProviderAdded,
                     manager, &MirSessionManager::onPromptProviderAdded);
    QObject::connect(listener, &PromptSessionListener::promptProviderRemoved,
                     manager, &MirSessionManager::onPromptProviderRemoved);
}

MirSessionManager* MirSessionManager::singleton()
{
    if (!the_session_manager) {

        NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

        if (!nativeInterface) {
            qCritical("ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin");
            QGuiApplication::quit();
            return nullptr;
        }

        SessionListener *sessionListener = static_cast<SessionListener*>(nativeInterface->nativeResourceForIntegration("SessionListener"));
        PromptSessionListener *promptSessionListener = static_cast<PromptSessionListener*>(nativeInterface->nativeResourceForIntegration("PromptSessionListener"));

        the_session_manager = new MirSessionManager(nativeInterface->m_mirConfig, ApplicationManager::singleton());

        connectToSessionListener(the_session_manager, sessionListener);
        connectToPromptSessionListener(the_session_manager, promptSessionListener);
    }
    return the_session_manager;
}

MirSessionManager::MirSessionManager(
        const QSharedPointer<MirServerConfiguration>& mirConfig,
        ApplicationManager* applicationManager,
        QObject *parent)
    : MirSessionItemModel(parent)
    , m_mirConfig(mirConfig)
    , m_applicationManager(applicationManager)
{
    qCDebug(QTMIR_SURFACES) << "MirSessionManager::MirSessionManager - this=" << this;
    setObjectName("qtmir::MirSessionManager");
}

MirSessionManager::~MirSessionManager()
{
    qCDebug(QTMIR_SURFACES) << "MirSessionManager::~MirSessionManager - this=" << this;
}

MirSessionItem *MirSessionManager::findSession(const mir::scene::Session* session) const
{
    for (MirSessionItem* child : list()) {
        if (child->session().get() == session)
            return child;
    }
    return nullptr;
}

void MirSessionManager::onSessionStarting(std::shared_ptr<mir::scene::Session> const& session)
{
    qCDebug(QTMIR_APPLICATIONS) << "MirSessionManager::onSessionStarting - sessionName=" <<  session->name().c_str();

    MirSessionItem* sessionItem = new MirSessionItem(session);
    insert(0, sessionItem);

    Application* application = m_applicationManager->findApplicationWithSession(session, false);
    if (application) {
        application->setSession(sessionItem);
    }

    Q_EMIT sessionStarting(sessionItem);
}

void MirSessionManager::onSessionStopping(std::shared_ptr<mir::scene::Session> const& session)
{
    qCDebug(QTMIR_APPLICATIONS) << "MirSessionManager::onSessionStopping - sessionName=" << session->name().c_str();

    MirSessionItem* sessionItem = findSession(session.get());
    remove(sessionItem);

    Q_EMIT sessionStopping(sessionItem);
}

void MirSessionManager::onPromptProviderAdded(const mir::scene::PromptSession *promptSession,
                                              const std::shared_ptr<mir::scene::Session> &session)
{
    Q_UNUSED(promptSession);
    Q_UNUSED(session);
}

void MirSessionManager::onPromptProviderRemoved(const mir::scene::PromptSession *promptSession,
                                                const std::shared_ptr<mir::scene::Session> &session)
{
    Q_UNUSED(promptSession);
    Q_UNUSED(session);
}

} // namespace qtmir
