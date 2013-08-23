/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef UBUNTU_APPLICATION_URL_DISPATCHER_SERVICE_H_
#define UBUNTU_APPLICATION_URL_DISPATCHER_SERVICE_H_

#include "ubuntu/application/url_dispatcher/session.h"

#include <ubuntu/visibility.h>

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * \defgroup url_dispatcher Functions and types to provide launching files/applications.
     *
     *  Applications in Ubuntu can not launch other applications directly.
     *  They have to use the url_dispatcher service to open other applications
     *  or to open applications that handle a given url.
     */

    /**
     * \brief Creates a new session with the url dispatcher service.
     * \returns A new session or NULL if the app lacks permissions to access the url dispatcher.
     */
    UBUNTU_DLL_PUBLIC UAUrlDispatcherSession*
    ua_url_dispatcher_session();

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_URL_DISPATCHER_SERVICE_H_
