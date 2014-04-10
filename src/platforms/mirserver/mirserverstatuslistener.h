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

#ifndef MIRSERVERSTATUSLISTENER_H
#define MIRSERVERSTATUSLISTENER_H

#include <mir/server_status_listener.h>

class MirServerStatusListener : public virtual mir::ServerStatusListener
{
public:
    void paused() override;
    void resumed() override;
    void started() override;
};

#endif // MIRSERVERSTATUSLISTENER_H
