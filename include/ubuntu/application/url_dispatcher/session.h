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

#ifndef UBUNTU_APPLICATION_URL_DISPATCHER_SESSION_H_
#define UBUNTU_APPLICATION_URL_DISPATCHER_SESSION_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Opaque type encapsulating a session with the url dispatcher service.
     * \ingroup url_dispatcher
     */
    typedef struct UbuntuApplicationUrlDispatcherSession UAUrlDispatcherSession;

    /**
     * \brief Callback type that is invoked for open results.
     * \ingroup url_dispatcher
     * \param[out] status U_STATUS_SUCCESS if the dispatching was successful.
     * \param[out] url The url passed to the ua_url_dispatcher_session_dispatch call.
     * \param[out] context The url context passed to the ua_url_dispatcher_session_dispatch call.
     */

    typedef void (*UAUrlDispatcherSessionDispatchHandler)(
        UStatus status,
        const char *url,
        void *context);

    /**
     * \brief Tells the url dispatcher to open the given url.
     * \ingroup url_dispatcher
     * \param[in] session The session instance.
     * \param[in] url The url to open.
     * \param[in] handler The url dispatcher open handler.
     * \param[in] context Your context, will be returned to you as is in the handler callback.
     */
    UBUNTU_DLL_PUBLIC void
    ua_url_dispatcher_session_open(
        UAUrlDispatcherSession *session,
        const char *url,
        UAUrlDispatcherSessionDispatchHandler handler,
        void *context);


#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_URL_DISPATCHER_SESSION_H_
