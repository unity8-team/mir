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

#ifndef MIRSESSIONITEM_H
#define MIRSESSIONITEM_H

// std
#include <memory>

// local
#include "mirsessionitemmodel.h"

// Qt
#include <QQuickItem>

// Unity API
#include <unity/shell/application/ApplicationInfoInterface.h>

namespace mir {
    namespace scene {
        class Snapshot;
        class Session;
    }
}

namespace qtmir {

class Application;

class MirSessionItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(MirSurfaceItem* surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(MirSessionItem* parentSession READ parentSession NOTIFY parentSessionChanged DESIGNABLE false)
    Q_PROPERTY(MirSessionItemModel* childSessions READ childSessions DESIGNABLE false CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)

public:
    explicit MirSessionItem(const std::shared_ptr<mir::scene::Session>& session,
                            QQuickItem *parent = 0);
    ~MirSessionItem();

    // Session State
    typedef unity::shell::application::ApplicationInfoInterface::State State;

    Q_INVOKABLE void release();

    //getters
    QString name() const;
    Application* application() const;
    MirSurfaceItem* surface() const;
    MirSessionItem* parentSession() const;
    State state() const;
    bool fullscreen() const;

    void setSession();
    void setApplication(Application* item);
    void setSurface(MirSurfaceItem* surface);
    void setScreenshot(const QUrl& m_screenshot);
    void setState(State state);

    void addChildSession(MirSessionItem* session);
    void insertChildSession(uint index, MirSessionItem* session);
    void removeChildSession(MirSessionItem* session);

    std::shared_ptr<mir::scene::Session> session() const;
    void takeSnapshot(std::function<void(mir::scene::Snapshot const& snapshot)>);

Q_SIGNALS:
    void surfaceChanged(MirSurfaceItem*);
    void parentSessionChanged(MirSessionItem*);
    void removed();
    void aboutToBeDestroyed();
    void stateChanged(State state);
    void fullscreenChanged(bool);

    void suspend();
    void resume();
    void respawn();

private Q_SLOTS:
    void updateFullscreenProperty();
    void discardSurface();

private:
    MirSessionItemModel* childSessions() const;
    void setParentSession(MirSessionItem* session);

    void setFullscreen(bool fullscreen);

    std::shared_ptr<mir::scene::Session> m_session;
    Application* m_application;
    MirSurfaceItem* m_surface;
    MirSessionItem* m_parentSession;
    MirSessionItemModel* m_children;
    bool m_fullscreen;
    State m_state;
    QTimer* m_suspendTimer;

    class Guard {};
    QSharedPointer<Guard> m_screenShotGuard;
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::MirSessionItem*)

#endif // MIRSESSIONITEM_H
