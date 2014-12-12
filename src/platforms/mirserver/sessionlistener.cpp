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
#include "surfaceobserver.h"
#include "logging.h"
#include "tracepoints.h" // generated from tracepoints.tp

#include <mir/scene/surface.h>

namespace ms = mir::scene;

Q_DECLARE_METATYPE(std::shared_ptr<ms::Session>)
Q_DECLARE_METATYPE(std::shared_ptr<ms::Surface>)

SessionListener::SessionListener(QObject *parent) :
    QObject(parent)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::SessionListener - this=" << this;
    // need to register type to send over threads with signal/slot
    qRegisterMetaType<std::shared_ptr<ms::Session>>("std::shared_ptr<mir::scene::Session>");
    qRegisterMetaType<std::shared_ptr<ms::Surface>>("std::shared_ptr<mir::scene::Surface>");
    qRegisterMetaType<std::shared_ptr<ms::SurfaceObserver>>("std::shared_ptr<mir::scene::SurfaceObserver>");
}

SessionListener::~SessionListener()
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::~SessionListener - this=" << this;
}

void SessionListener::starting(std::shared_ptr<ms::Session> const& session)
{
    tracepoint(qtmirserver, starting);
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::starting - this=" << this << "session=" << session.get();
    Q_EMIT sessionStarting(session);
}

void SessionListener::stopping(std::shared_ptr<ms::Session> const& session)
{
    tracepoint(qtmirserver, stopping);
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::stopping - this=" << this << "session=" << session.get();
    Q_EMIT sessionStopping(session);
}

void SessionListener::focused(std::shared_ptr<ms::Session> const& session)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::focused - this=" << this << "session=" << session.get();
    Q_EMIT sessionFocused(session);
}

void SessionListener::unfocused()
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::unfocused - this=" << this;
    Q_EMIT sessionUnfocused();
}

void SessionListener::surface_created(ms::Session& session, std::shared_ptr<ms::Surface> const& surface)
{
    tracepoint(qtmirserver, surfaceCreated);
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::surface_created - this=" << this << "session=" << &session
                                   << "surface=" << surface.get();
    std::shared_ptr<SurfaceObserver> surfaceObserver = std::make_shared<SurfaceObserver>();
    surface->add_observer(surfaceObserver);
    Q_EMIT sessionCreatedSurface(&session, surface, surfaceObserver);
}

void SessionListener::destroying_surface(ms::Session& session, std::shared_ptr<ms::Surface> const& surface)
{
    tracepoint(qtmirserver, surfaceDestroyed);
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionListener::destroying_surface - this=" << this << "session=" << &session
                                   << "surface=" << surface.get();
    Q_EMIT sessionDestroyingSurface(&session, surface);
}
