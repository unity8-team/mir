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

#include "sessionlistener.h"
#include "logging.h"

namespace msh = mir::shell;

Q_DECLARE_METATYPE(std::shared_ptr<msh::Session>)
Q_DECLARE_METATYPE(std::shared_ptr<msh::Surface>)

SessionListener::SessionListener(QObject *parent) :
    QObject(parent)
{
    DLOG("SessionListener::SessionListener (this=%p)", this);
    // need to register type to send over threads with signal/slot
    qRegisterMetaType<std::shared_ptr<msh::Session>>("std::shared_ptr<mir::shell::Session>");
    qRegisterMetaType<std::shared_ptr<msh::Surface>>("std::shared_ptr<mir::shell::Surface>");
}

SessionListener::~SessionListener()
{
    DLOG("SessionListener::~SessionListener (this=%p)", this);
}

void SessionListener::starting(std::shared_ptr<msh::Session> const& session)
{
    DLOG("SessionListener::starting (this=%p, session=%p)", this, (void*)session.get());
    Q_EMIT sessionStarting(session);
}

void SessionListener::stopping(std::shared_ptr<msh::Session> const& session)
{
    DLOG("SessionListener::stopping (this=%p, session=%p)", this, (void*)session.get());
    Q_EMIT sessionStopping(session);
}

void SessionListener::focused(std::shared_ptr<msh::Session> const& session)
{
    DLOG("SessionListener::focused (this=%p, session=%p)", this, (void*)session.get());
    Q_EMIT sessionFocused(session);
}

void SessionListener::unfocused()
{
    DLOG("SessionListener::unfocused (this=%p)", this);
    Q_EMIT sessionUnfocused();
}

void SessionListener::surface_created(msh::Session& session, std::shared_ptr<msh::Surface> const& surface)
{
    DLOG("SessionListener::surface_created (this=%p, session=%p, surface=%p)", this, &session, (void*)surface.get());
    Q_EMIT sessionCreatedSurface(&session, surface);
}

void SessionListener::destroying_surface(msh::Session& session, std::shared_ptr<mir::shell::Surface> const& surface)
{
    DLOG("SessionListener::destroying_surface (this=%p, session=%p, surface=%p)", this, &session, (void*)surface.get());
    Q_EMIT sessionDestroyingSurface(&session, surface);
}
