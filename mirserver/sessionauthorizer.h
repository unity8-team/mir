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

#ifndef SESSIONAUTHORIZER_H
#define SESSIONAUTHORIZER_H

//std
#include <string>

// mir
#include <mir/frontend/session_authorizer.h>

// Qt
#include <QObject>

class SessionAuthorizer : public QObject, public mir::frontend::SessionAuthorizer
{
    Q_OBJECT

public:
    SessionAuthorizer(QObject *parent = 0);
    ~SessionAuthorizer();

    virtual bool connection_is_allowed(pid_t pid) override;
    virtual bool configure_display_is_allowed(pid_t pid) override;

Q_SIGNALS:
    // needs to be blocked queued signal which returns value for authorized
    void requestAuthorizationForSession(const quint64 &pid, bool &authorized);
};

#endif // SESSIONAUTHORIZER_H
