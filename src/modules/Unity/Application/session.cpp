/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// local
#include "application.h"
#include "debughelpers.h"
#include "session.h"
#include "mirsurfacemanager.h"
#include "mirsurfaceitem.h"

// mirserver
#include "logging.h"

// mir
#include <mir/scene/session.h>
#include <mir/scene/prompt_session_manager.h>

// Qt
#include <QPainter>
#include <QQmlEngine>

namespace ms = mir::scene;

namespace qtmir
{

Session::Session(const std::shared_ptr<ms::Session>& session,
                 const std::shared_ptr<ms::PromptSessionManager>& promptSessionManager,
                 QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_application(nullptr)
    , m_surface(nullptr)
    , m_parentSession(nullptr)
    , m_children(new SessionModel(this))
    , m_fullscreen(false)
    , m_state(State::Starting)
    , m_suspendTimer(new QTimer(this))
    , m_promptSessionManager(promptSessionManager)
{
    qCDebug(QTMIR_SESSIONS) << "Session::Session() " << this->name();

    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    m_suspendTimer->setSingleShot(true);
    connect(m_suspendTimer, &QTimer::timeout, this, [this]() {
        if (m_surface) {
            m_surface->stopFrameDropper();
        } else {
            qDebug() << "Application::suspend - no surface to call stopFrameDropper() on!";
        }
        Q_EMIT suspended();
    });
}

Session::~Session()
{
    qCDebug(QTMIR_SESSIONS) << "Session::~Session() " << name();
    stopPromptSessions();

    QList<Session*> children(m_children->list());
    for (Session* child : children) {
        delete child;
    }
    if (m_parentSession) {
        m_parentSession->removeChildSession(this);
    }
    if (m_application) {
        m_application->setSession(nullptr);
    }
    delete m_surface; m_surface = nullptr;
    delete m_children; m_children = nullptr;
}

void Session::release()
{
    qCDebug(QTMIR_SESSIONS) << "Session::release " << name();
    Q_EMIT aboutToBeDestroyed();

    if (m_parentSession) {
        m_parentSession->removeChildSession(this);
    }
    if (m_application) {
        m_application->setSession(nullptr);
    }
    if (!parent()) {
        deleteLater();
    }
}

QString Session::name() const
{
    return QString::fromStdString(m_session->name());
}

std::shared_ptr<ms::Session> Session::session() const
{
    return m_session;
}

Application* Session::application() const
{
    return m_application;
}

MirSurfaceItem* Session::surface() const
{
    // Only notify QML of surface creation once it has drawn its first frame.
    if (m_surface && m_surface->isFirstFrameDrawn()) {
        return m_surface;
    } else {
        return nullptr;
    }
}

Session* Session::parentSession() const
{
    return m_parentSession;
}

Session::State Session::state() const
{
    return m_state;
}

bool Session::fullscreen() const
{
    return m_fullscreen;
}

void Session::setApplication(Application* application)
{
    if (m_application == application)
        return;

    m_application = application;
}

void Session::setSurface(MirSurfaceItem *newSurface)
{
    qCDebug(QTMIR_SESSIONS) << "Session::setSurface - session=" << name() << "surface=" << newSurface;

    if (newSurface == m_surface) {
        return;
    }

    if (m_surface) {
        m_surface->disconnect(this);
        m_surface->setSession(nullptr);
        m_surface->setParent(nullptr);
    }

    MirSurfaceItem *previousSurface = surface();
    m_surface = newSurface;

    if (newSurface) {
        m_surface->setParent(this);
        m_surface->setSession(this);

        // Only notify QML of surface creation once it has drawn its first frame.
        if (!surface()) {
            connect(newSurface, &MirSurfaceItem::firstFrameDrawn,
                    this, [this] { Q_EMIT surfaceChanged(m_surface); });
        }

        connect(newSurface, &MirSurfaceItem::stateChanged,
            this, &Session::updateFullscreenProperty);
    }

    if (previousSurface != surface()) {
        Q_EMIT surfaceChanged(m_surface);
    }

    updateFullscreenProperty();
}

void Session::updateFullscreenProperty()
{
    if (m_surface) {
        setFullscreen(m_surface->state() == MirSurfaceItem::Fullscreen);
    } else {
        // Keep the current value of the fullscreen property until we get a new
        // surface
    }
}

void Session::setFullscreen(bool fullscreen)
{
    qCDebug(QTMIR_SESSIONS) << "Session::setFullscreen - session=" << this << "fullscreen=" << fullscreen;
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        Q_EMIT fullscreenChanged(m_fullscreen);
    }
}

void Session::setState(State state)
{
    qCDebug(QTMIR_SESSIONS) << "Session::setState - session=" << this << "state=" << applicationStateToStr(state);
    if (m_state != state) {
        switch (state)
        {
        case Session::State::Suspended:
            if (m_state == Session::State::Running) {
                stopPromptSessions();
                session()->set_lifecycle_state(mir_lifecycle_state_will_suspend);
                m_suspendTimer->start(3000);
            }
            break;
        case Session::State::Running:
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();

            if (m_state == Session::State::Suspended) {
                if (m_surface)
                    m_surface->startFrameDropper();
                Q_EMIT resumed();
                session()->set_lifecycle_state(mir_lifecycle_state_resumed);
            } else if (m_state == Session::State::Stopped) {
                Q_EMIT respawned();
                state = Session::State::Starting;
            }
            break;
        case Session::State::Stopped:
            stopPromptSessions();
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();
            if (m_surface)
                m_surface->stopFrameDropper();
            break;
        default:
            break;
        }

        m_state = state;
        Q_EMIT stateChanged(state);

        foreachChildSession([state](Session* session) {
            session->setState(state);
        });
    }
}

void Session::setParentSession(Session* session)
{
    if (m_parentSession == session || session == this)
        return;

    m_parentSession = session;

    Q_EMIT parentSessionChanged(session);
}

void Session::addChildSession(Session* session)
{
    insertChildSession(m_children->rowCount(), session);
}

void Session::insertChildSession(uint index, Session* session)
{
    qCDebug(QTMIR_SESSIONS) << "Session::insertChildSession - " << session->name() << " to " << name() << " @  " << index;

    session->setParentSession(this);
    m_children->insert(index, session);

    session->setState(state());
}

void Session::removeChildSession(Session* session)
{
    qCDebug(QTMIR_SESSIONS) << "Session::removeChildSession - " << session->name() << " from " << name();

    if (m_children->contains(session)) {
        m_children->remove(session);
        session->setParentSession(nullptr);
    }
}

void Session::foreachChildSession(std::function<void(Session* session)> f) const
{
    QList<Session*> children(m_children->list());
    for (Session* child : children) {
        f(child);
    }
}

SessionModel* Session::childSessions() const
{
    return m_children;
}

void Session::appendPromptSession(const std::shared_ptr<ms::PromptSession>& promptSession)
{
    qCDebug(QTMIR_SESSIONS) << "Session::appendPromptSession session=" << name()
            << "promptSession=" << (promptSession ? promptSession.get() : nullptr);

    m_promptSessions.append(promptSession);
}

void Session::removePromptSession(const std::shared_ptr<ms::PromptSession>& promptSession)
{
    qCDebug(QTMIR_SESSIONS) << "Session::removePromptSession session=" << name()
            << "promptSession=" << (promptSession ? promptSession.get() : nullptr);

    m_promptSessions.removeAll(promptSession);
}

void Session::stopPromptSessions()
{
    QList<Session*> children(m_children->list());
    for (Session* child : children) {
        child->stopPromptSessions();
    }

    QList<std::shared_ptr<ms::PromptSession>> copy(m_promptSessions);
    QListIterator<std::shared_ptr<ms::PromptSession>> it(copy);
    for ( it.toBack(); it.hasPrevious(); ) {
        m_promptSessionManager->stop_prompt_session(it.previous());
    }
}

std::shared_ptr<ms::PromptSession> Session::activePromptSession() const
{
    if (m_promptSessions.count() > 0)
        return m_promptSessions.back();
    return nullptr;
}

void Session::foreachPromptSession(std::function<void(const std::shared_ptr<ms::PromptSession>&)> f) const
{
    for (std::shared_ptr<ms::PromptSession> promptSession : m_promptSessions) {
        f(promptSession);
    }
}

} // namespace qtmir
