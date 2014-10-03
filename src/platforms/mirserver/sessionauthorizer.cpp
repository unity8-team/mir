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
#include "tracepoints.h" // generated from tracepoints.tp

#include <QMetaMethod>
#include <QThread>

// mir
#include <mir/frontend/session_credentials.h>

using mir::frontend::SessionCredentials;

SessionAuthorizer::SessionAuthorizer(QObject *parent)
    : QObject(parent)
    , m_connectionChecked(false)
{
}

SessionAuthorizer::~SessionAuthorizer()
{
}

bool SessionAuthorizer::connection_is_allowed(SessionCredentials const& creds)
{
    tracepoint(qtmirserver, sessionAuthorizeStart);
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionAuthorizer::connection_is_allowed - this=" << this << "pid=" << creds.pid();
    bool authorized = true;

    if (!m_connectionChecked) {
        const QMetaObject *mo = metaObject();
        QMetaMethod mm = mo->method(mo->indexOfSignal("requestAuthorizationForSession(quint64,bool&)"));
        for (int i = 0; i < 100 && !isSignalConnected(mm); ++i) {
            QThread::usleep(10000);
        }
        if (!isSignalConnected(mm)) {
            qCDebug(QTMIR_MIR_MESSAGES) << "SessionAuthorizer::connection_is_allowed - Gave up waiting for signal listeners";
        }
        m_connectionChecked = true;
    }

    Q_EMIT requestAuthorizationForSession(creds.pid(), authorized); // needs to block until authorized value returned
    tracepoint(qtmirserver, sessionAuthorizeEnd);

    return authorized;
}

bool SessionAuthorizer::configure_display_is_allowed(SessionCredentials const& creds)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionAuthorizer::configure_display_is_allowed - this=" << this << "pid=" << creds.pid();

    //FIXME(ricmm) Actually mediate this access for clients
    Q_UNUSED(creds)
    return true;
}

bool SessionAuthorizer::screencast_is_allowed(SessionCredentials const& creds)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionAuthorizer::screencast_is_allowed - this=" << this << "pid=" << creds.pid();

    //FIXME Actually mediate this access for clients
    Q_UNUSED(creds)
    return true;
}

bool SessionAuthorizer::prompt_session_is_allowed(SessionCredentials const& creds)
{
    qCDebug(QTMIR_MIR_MESSAGES) << "SessionAuthorizer::prompt_session_is_allowed - this=" << this << "pid=" << creds.pid();

    //FIXME Actually mediate this access for clients
    Q_UNUSED(creds)
    return true;
}
