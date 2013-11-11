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

#include "sessionauthorizer.h"
#include "logging.h"

SessionAuthorizer::SessionAuthorizer(QObject *parent)
    :   QObject(parent)
{
}

SessionAuthorizer::~SessionAuthorizer()
{
}

bool SessionAuthorizer::connection_is_allowed(pid_t pid)
{
    DLOG("SessionAuthorizer::connection_is_allowed (this=%p, pid=%d", this, pid);
    bool authorized = true;

    Q_EMIT requestAuthorizationForSession(pid, authorized); // needs to block until authorized value returned
    return authorized;
}

bool SessionAuthorizer::configure_display_is_allowed(pid_t pid)
{
    DLOG("SessionAuthorizer::configure_display_is_allowed (this=%p, pid=%d)", this, pid);

    //FIXME(ricmm) Actually mediate this access for clients
    Q_UNUSED(pid)
    return true;
}
