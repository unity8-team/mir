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
#include "mirsessionitem.h"
#include "mirsurfacemanager.h"
#include "mirsurfaceitem.h"

// mirserver
#include "logging.h"

// mir
#include <mir/scene/session.h>

// Qt
#include <QPainter>
#include <QQmlEngine>
#include <QTimer>

namespace qtmir
{
QMutex screenshotMutex;

MirSessionItem::MirSessionItem(const std::shared_ptr<mir::scene::Session>& session, QQuickItem *parent)
    : QQuickItem(parent)
    , m_session(session)
    , m_application(nullptr)
    , m_surface(nullptr)
    , m_parentSession(nullptr)
    , m_children(new MirSessionItemModel(this))
    , m_fullscreen(false)
    , m_state(State::Starting)
    , m_suspendTimer(new QTimer(this))
    , m_screenShotGuard(new Guard)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::MirSessionItem() " << this->name();

    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    m_suspendTimer->setSingleShot(true);
    connect(m_suspendTimer, &QTimer::timeout, this, [this]() {
        if (m_surface) {
            m_surface->stopFrameDropper();
        } else {
            qDebug() << "Application::suspend - no surface to call stopFrameDropper() on!";
        }
        Q_EMIT suspend();
    });
}

MirSessionItem::~MirSessionItem()
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::~MirSessionItem() " << name();
    {
        // In case we get a threaded screenshot callback once the application is deleted.
        QMutexLocker lk(&screenshotMutex);
        m_screenShotGuard.clear();
    }

    QList<MirSessionItem*> children(m_children->list());
    for (MirSessionItem* child : children) {
        delete child;
    }
    if (m_parentSession) {
        m_parentSession->removeChildSession(this);
    }
    if (m_application) {
        m_application->setSession(nullptr);
    }
    delete m_surface;
    delete m_children;
}

void MirSessionItem::release()
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::release " << name();
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

QString MirSessionItem::name() const
{
    return QString::fromStdString(m_session->name());
}

std::shared_ptr<mir::scene::Session> MirSessionItem::session() const
{
    return m_session;
}

Application* MirSessionItem::application() const
{
    return m_application;
}

MirSurfaceItem* MirSessionItem::surface() const
{
    // Only notify QML of surface creation once it has drawn its first frame.
    if (m_surface && m_surface->isFirstFrameDrawn()) {
        return m_surface;
    } else {
        return nullptr;
    }
}

MirSessionItem* MirSessionItem::parentSession() const
{
    return m_parentSession;
}

MirSessionItem::State MirSessionItem::state() const
{
    return m_state;
}

bool MirSessionItem::fullscreen() const
{
    return m_fullscreen;
}

void MirSessionItem::setApplication(Application* application)
{
    if (m_application == application)
        return;

    m_application = application;
}

void MirSessionItem::setSurface(MirSurfaceItem *newSurface)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::setSurface - session=" << name() << "surface=" << newSurface;

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

        connect(newSurface, &MirSurfaceItem::surfaceDestroyed,
                this, &MirSessionItem::discardSurface);

        connect(newSurface, &MirSurfaceItem::stateChanged,
            this, &MirSessionItem::updateFullscreenProperty);
    }

    if (previousSurface != surface()) {
        Q_EMIT surfaceChanged(m_surface);
    }

    updateFullscreenProperty();
}

void MirSessionItem::updateFullscreenProperty()
{
    setFullscreen(m_surface && m_surface->state() == MirSurfaceItem::Fullscreen);
}

void MirSessionItem::discardSurface()
{
    MirSurfaceItem *discardedSurface = m_surface;
    setSurface(nullptr);
    delete discardedSurface;
}

void MirSessionItem::setFullscreen(bool fullscreen)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::setFullscreen - session=" << this << "fullscreen=" << fullscreen;
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        Q_EMIT fullscreenChanged(m_fullscreen);
    }
}

void MirSessionItem::setParentSession(MirSessionItem* session)
{
    if (m_parentSession == session || session == this)
        return;

    m_parentSession = session;

    Q_EMIT parentSessionChanged(session);
}

void MirSessionItem::setState(State state)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::setState - session=" << this << "state=" << applicationStateToStr(state);
    if (m_state != state) {
        switch (state)
        {
        case MirSessionItem::State::Suspended:
            if (m_state == MirSessionItem::State::Running) {
                session()->set_lifecycle_state(mir_lifecycle_state_will_suspend);
                m_suspendTimer->start(3000);
            }
            break;
        case MirSessionItem::State::Running:
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();

            if (m_state == MirSessionItem::State::Suspended) {
                if (m_surface)
                    m_surface->startFrameDropper();
                Q_EMIT resume();
                session()->set_lifecycle_state(mir_lifecycle_state_resumed);
            } else if (m_state == MirSessionItem::State::Stopped) {
                Q_EMIT respawn();
                state = MirSessionItem::State::Starting;
            }
            break;
        case MirSessionItem::State::Stopped:
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
    }
}

void MirSessionItem::addChildSession(MirSessionItem* session)
{
    insertChildSession(m_children->rowCount(), session);
}

void MirSessionItem::insertChildSession(uint index, MirSessionItem* session)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::insertChildSession - " << session->name() << " to " << name() << " @  " << index;

    session->setParentSession(this);
    m_children->insert(index, session);
}

void MirSessionItem::removeChildSession(MirSessionItem* session)
{
    qCDebug(QTMIR_SESSIONS) << "MirSessionItem::removeChildSession - " << session->name() << " from " << name();

    if (m_children->contains(session)) {
        m_children->remove(session);
        session->setParentSession(nullptr);
    }
}

MirSessionItemModel* MirSessionItem::childSessions() const
{
    return m_children;
}

void MirSessionItem::takeSnapshot(std::function<void(mir::scene::Snapshot const&)> f)
{
    QWeakPointer<Guard> wk(m_screenShotGuard.toWeakRef());

    m_session->take_snapshot([wk, f](mir::scene::Snapshot const& snapshot) {
        // In case we get a threaded screenshot callback once the application is deleted.
        QMutexLocker lk(&screenshotMutex);
        if (wk.isNull())
            return;

        f(snapshot);
    });
}

} // namespace qtmir
