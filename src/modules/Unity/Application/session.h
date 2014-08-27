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

#ifndef SESSION_H
#define SESSION_H

// std
#include <memory>

// local
#include "sessionmodel.h"

// Qt
#include <QObject>
#include <QTimer>

// Unity API
#include <unity/shell/application/ApplicationInfoInterface.h>

namespace mir {
    namespace scene {
        class Session;
        class PromptSession;
        class PromptSessionManager;
    }
}

namespace qtmir {

class Application;
class MirSurfaceItem;

class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MirSurfaceItem* surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(Session* parentSession READ parentSession NOTIFY parentSessionChanged DESIGNABLE false)
    Q_PROPERTY(SessionModel* childSessions READ childSessions DESIGNABLE false CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)

public:
    explicit Session(const std::shared_ptr<mir::scene::Session>& session,
                     const std::shared_ptr<mir::scene::PromptSessionManager>& promptSessionManager,
                     QObject *parent = 0);
    ~Session();

    // Session State
    typedef unity::shell::application::ApplicationInfoInterface::State State;

    Q_INVOKABLE void release();

    //getters
    QString name() const;
    Application* application() const;
    MirSurfaceItem* surface() const;
    Session* parentSession() const;
    State state() const;
    bool fullscreen() const;

    void setSession();
    void setApplication(Application* item);
    void setSurface(MirSurfaceItem* surface);
    void setState(State state);

    void addChildSession(Session* session);
    void insertChildSession(uint index, Session* session);
    void removeChildSession(Session* session);

    std::shared_ptr<mir::scene::Session> session() const;

    std::shared_ptr<mir::scene::PromptSession> activePromptSession() const;
    void foreachPromptSession(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f) const;

Q_SIGNALS:
    void surfaceChanged(MirSurfaceItem*);
    void parentSessionChanged(Session*);
    void removed();
    void aboutToBeDestroyed();
    void stateChanged(State state);
    void fullscreenChanged(bool fullscreen);

    void suspend();
    void resume();
    void respawn();

private Q_SLOTS:
    void updateFullscreenProperty();
    void discardSurface();

private:
    SessionModel* childSessions() const;
    void setParentSession(Session* session);

    void appendPromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void removePromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void stopPromptSessions();

    void setFullscreen(bool fullscreen);

    std::shared_ptr<mir::scene::Session> m_session;
    Application* m_application;
    MirSurfaceItem* m_surface;
    Session* m_parentSession;
    SessionModel* m_children;
    bool m_fullscreen;
    State m_state;
    QTimer* m_suspendTimer;
    QList<std::shared_ptr<mir::scene::PromptSession>> m_promptSessions;
    std::shared_ptr<mir::scene::PromptSessionManager> const m_promptSessionManager;

    friend class SessionManager;
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::Session*)

#endif // SESSION_H
