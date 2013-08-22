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

#include "ubuntu/application/url_dispatcher/service.h"
#include "session_p.h"

#include <stdlib.h>

UAUrlDispatcherSession*
ua_url_dispatcher_session()
{
    UAUrlDispatcherSession *session = (UAUrlDispatcherSession*)malloc(sizeof(UAUrlDispatcherSession));
    return session;
}
