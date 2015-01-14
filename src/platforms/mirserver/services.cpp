/*
 * Copyright (C) 2014 Canonical, Ltd.
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

#include "services.h"

#include <QUrl>
#include <QByteArray>

#include <url-dispatcher.h>

bool Services::openUrl(const QUrl &url)
{
    return callDispatcher(url);
}

bool Services::openDocument(const QUrl &url)
{
    return callDispatcher(url);
}

bool Services::callDispatcher(const QUrl &qUrl)
{
    QByteArray encoded = qUrl.toEncoded();
    const char *url = encoded.constData();

    url_dispatch_send(url, nullptr /*dispatch_callback*/, nullptr /*callback_data*/);

    // We are returning true here because the other option
    // is spawning a nested event loop and wait for the
    // callback. But there is no guarantee on how fast
    // the callback is going to be so we prefer to avoid the
    // nested event loop. Long term plan is improve Qt API
    // to support an async openUrl
    return true;
}
