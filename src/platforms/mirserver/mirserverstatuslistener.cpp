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

// local
#include "mirserverstatuslistener.h"

// Qt
#include <QtGlobal>
#include <QByteArray>

// std
#include <csignal>

void MirServerStatusListener::paused()
{
}

void MirServerStatusListener::resumed()
{
}

void MirServerStatusListener::started()
{
    // Emit signal to notify Upstart that Mir is ready to receive client connections
    // see http://upstart.ubuntu.com/cookbook/#expect-stop
    // FIXME: should not be qtmir's job, instead should notify the user of this library
    // that they should emit this signal, perhaps by posting an event to the
    // QMirServerApplication event loop when it comes up
    if (qgetenv("UNITY_MIR_EMITS_SIGSTOP") == "1") {
        raise(SIGSTOP);
    }
}
