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

#ifndef SESSION_INTERFACE_H
#define SESSION_INTERFACE_H

#include <functional>
#include <memory>

// Unity API
#include <unity/shell/application/ApplicationInfoInterface.h>

// local
#include "sessionmodel.h"

namespace mir {
    namespace scene {
        class Session;
        class PromptSession;
    }
}

namespace qtmir {

class MirSurfaceItem;

// TODO: Move it to unity-api
class SessionInterface : public QObject {
    Q_OBJECT
    Q_PROPERTY(MirSurfaceItem* surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(unity::shell::application::ApplicationInfoInterface* application READ application NOTIFY applicationChanged DESIGNABLE false)
    Q_PROPERTY(SessionInterface* parentSession READ parentSession NOTIFY parentSessionChanged DESIGNABLE false)
    Q_PROPERTY(SessionModel* childSessions READ childSessions DESIGNABLE false CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool live READ live NOTIFY liveChanged)
public:
    SessionInterface(QObject *parent = 0) : QObject(parent) {}
    virtual ~SessionInterface() {}

    // Session State
    typedef unity::shell::application::ApplicationInfoInterface::State State;

    Q_INVOKABLE virtual void release() = 0;

    //getters
    virtual QString name() const = 0;
    virtual unity::shell::application::ApplicationInfoInterface* application() const = 0;
    virtual MirSurfaceItem* surface() const = 0;
    virtual SessionInterface* parentSession() const = 0;
    virtual State state() const = 0;
    virtual bool fullscreen() const = 0;
    virtual bool live() const = 0;

    virtual void setApplication(unity::shell::application::ApplicationInfoInterface* item) = 0;
    virtual void setSurface(MirSurfaceItem* surface) = 0;
    virtual void setState(State state) = 0;

    virtual void addChildSession(SessionInterface* session) = 0;
    virtual void insertChildSession(uint index, SessionInterface* session) = 0;
    virtual void removeChildSession(SessionInterface* session) = 0;
    virtual void foreachChildSession(std::function<void(SessionInterface* session)> f) const = 0;

    virtual std::shared_ptr<mir::scene::Session> session() const = 0;

    virtual std::shared_ptr<mir::scene::PromptSession> activePromptSession() const = 0;
    virtual void foreachPromptSession(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f) const = 0;

    virtual SessionModel* childSessions() const = 0;

Q_SIGNALS:
    void surfaceChanged(MirSurfaceItem*);
    void parentSessionChanged(SessionInterface*);
    void applicationChanged(unity::shell::application::ApplicationInfoInterface* application);
    void aboutToBeDestroyed();
    void stateChanged(State state);
    void fullscreenChanged(bool fullscreen);
    void liveChanged(bool live);

    void suspended();
    void resumed();

protected:
    virtual void setFullscreen(bool fullscreen) = 0;
    virtual void setLive(const bool) = 0;
    virtual void appendPromptSession(const std::shared_ptr<mir::scene::PromptSession>& session) = 0;
    virtual void removePromptSession(const std::shared_ptr<mir::scene::PromptSession>& session) = 0;

    friend class SessionManager;
};

} // namespace qtmir


#endif // SESSION_INTERFACE_H
