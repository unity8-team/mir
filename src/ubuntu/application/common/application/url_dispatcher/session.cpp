/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ubuntu/application/url_dispatcher/session.h"

#include <url-dispatcher.h>

#include <stdlib.h>

struct DispatchCallBackData
{
    UAUrlDispatcherSessionDispatchHandler handler;
    void *user_context;
};

static
void
dispatch_callback(const gchar *url, gboolean success, gpointer user_data)
{
    DispatchCallBackData *data = (DispatchCallBackData*)user_data;
    if (data->handler) 
        data->handler(success ? U_STATUS_SUCCESS : U_STATUS_ERROR, url, data->user_context);
    free(data);
}

void
ua_url_dispatcher_session_open(
    UAUrlDispatcherSession *session,
    const char *url,
    UAUrlDispatcherSessionDispatchHandler handler,
    void *user_context)
{
    DispatchCallBackData *data = (DispatchCallBackData*)malloc(sizeof(DispatchCallBackData));
    data->handler = handler;
    data->user_context = user_context;
    url_dispatch_send(url, dispatch_callback, data);
}
