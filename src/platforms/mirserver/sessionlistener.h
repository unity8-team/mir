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

#include "mir/shell/session_listener.h"
#include "mir/shell/session.h"

class SessionListener : public QObject, public mir::shell::SessionListener
{
    Q_OBJECT
public:
    explicit SessionListener(QObject *parent = 0);
    ~SessionListener();

    void starting(std::shared_ptr<mir::shell::Session> const& session) override;
    void stopping(std::shared_ptr<mir::shell::Session> const& session) override;
    void focused(std::shared_ptr<mir::shell::Session> const& session) override;
    void unfocused() override;

    void surface_created(mir::shell::Session&, std::shared_ptr<mir::shell::Surface> const&) override;
    void destroying_surface(mir::shell::Session&, std::shared_ptr<mir::shell::Surface> const&) override;

Q_SIGNALS:
    void sessionStarting(std::shared_ptr<mir::shell::Session> const& session);
    void sessionStopping(std::shared_ptr<mir::shell::Session> const& session);
    void sessionFocused(std::shared_ptr<mir::shell::Session> const& session);
    void sessionUnfocused();

    void sessionCreatedSurface(mir::shell::Session const*, std::shared_ptr<mir::shell::Surface> const&);
    void sessionDestroyingSurface(mir::shell::Session const*, std::shared_ptr<mir::shell::Surface> const&);
};

#endif // SESSIONLISTENER_H
