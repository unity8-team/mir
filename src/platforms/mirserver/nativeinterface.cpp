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
 *
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#include "nativeinterface.h"

NativeInterface::NativeInterface(MirServerConfiguration *config)
    : m_mirConfig(config)
{
}

void *NativeInterface::nativeResourceForIntegration(const QByteArray &resource)
{
    void *result = nullptr;

    if (resource == "SessionAuthorizer")
        result = m_mirConfig->sessionAuthorizer();
    else if (resource == "SessionConfigurator")
        result = m_mirConfig->surfaceConfigurator();
    else if (resource == "SessionListener")
        result = m_mirConfig->sessionListener();

    return result;
}
