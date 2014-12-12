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

#ifndef SESSIONLISTENER_H
#define SESSIONLISTENER_H

#include <QObject>

#include "mir/scene/session_listener.h"
#include "mir/scene/session.h"
#include "mir/scene/surface_observer.h"

class SessionListener : public QObject, public mir::scene::SessionListener
{
    Q_OBJECT
public:
    explicit SessionListener(QObject *parent = 0);
    ~SessionListener();

    void starting(std::shared_ptr<mir::scene::Session> const& session) override;
    void stopping(std::shared_ptr<mir::scene::Session> const& session) override;
    void focused(std::shared_ptr<mir::scene::Session> const& session) override;
    void unfocused() override;

    void surface_created(mir::scene::Session&, std::shared_ptr<mir::scene::Surface> const&) override;
    void destroying_surface(mir::scene::Session&, std::shared_ptr<mir::scene::Surface> const&) override;

Q_SIGNALS:
    void sessionStarting(std::shared_ptr<mir::scene::Session> const& session);
    void sessionStopping(std::shared_ptr<mir::scene::Session> const& session);
    void sessionFocused(std::shared_ptr<mir::scene::Session> const& session);
    void sessionUnfocused();

    void sessionCreatedSurface(mir::scene::Session const*, std::shared_ptr<mir::scene::Surface> const&, std::shared_ptr<mir::scene::SurfaceObserver> const&);
    void sessionDestroyingSurface(mir::scene::Session const*, std::shared_ptr<mir::scene::Surface> const&);
};

#endif // SESSIONLISTENER_H
