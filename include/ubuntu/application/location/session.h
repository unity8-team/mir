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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_LOCATION_SESSION_H_
#define UBUNTU_APPLICATION_LOCATION_SESSION_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#include <ubuntu/application/location/heading_update.h>
#include <ubuntu/application/location/position_update.h>
#include <ubuntu/application/location/velocity_update.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Opaque type encapsulating a session with the location service.
     * \ingroup location_service
     */
    typedef struct UbuntuApplicationLocationServiceSession UALocationServiceSession;

    /**
     * \brief Callback type that is invoked for position updates.
     * \ingroup location_service
     */
    typedef void (*UALocationServiceSessionPositionUpdatesHandler)(
        UALocationPositionUpdate *position,
        void *context);

    /**
     * \brief Callback type that is invoked for heading updates.
     * \ingroup location_service
     */
    typedef void (*UALocationServiceSessionHeadingUpdatesHandler)(
        UALocationHeadingUpdate *heading,
        void *context);

    /**
     * \brief Callback type that is invoked for velocity updates.
     * \ingroup location_service
     */
    typedef void (*UALocationServiceSessionVelocityUpdatesHandler)(
        UALocationVelocityUpdate *heading,
        void *context);

    /**
     * \brief Increments the reference count of the session instance.
     * \ingroup location_service
     * \param[in] session The session instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_ref(
        UALocationServiceSession *session);

    /**
     * \brief Decrements the reference count of the session instance.
     * \ingroup location_service
     * \param[in] session The session instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_unref(
        UALocationServiceSession *session);

    /**
     * \brief Installs an app-specific position update handler for the session.
     * \ingroup location_service
     * \param[in] session The session instance to install the handler for.
     * \param[in] handler The position update handler.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_set_position_updates_handler(
        UALocationServiceSession *session,
        UALocationServiceSessionPositionUpdatesHandler handler,
        void *context);

    /**
     * \brief Installs an app-specific heading update handler for the session.
     * \ingroup location_service
     * \param[in] session The session instance to install the handler for.
     * \param[in] handler The heading update handler.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_set_heading_updates_handler(
        UALocationServiceSession *session,
        UALocationServiceSessionHeadingUpdatesHandler handler,
        void *context);

    /**
     * \brief Installs an app-specific velocity update handler for the session.
     * \ingroup location_service
     * \param[in] session The session instance to install the handler for.
     * \param[in] handler The velocity update handler.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_set_velocity_updates_handler(
        UALocationServiceSession *session,
        UALocationServiceSessionVelocityUpdatesHandler handler,
        void *context);

    /**
     * \brief Starts position updates for the supplied session.
     * \ingroup location_service
     * \returns U_STATUS_SUCCESS if the updates were successfully started on the service side, else U_STATUS_ERROR.
     * \param[in] session The session instance to start position updates for.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_session_start_position_updates(
        UALocationServiceSession *session);

    /**
     * \brief Stops position updates for the supplied session.
     * \ingroup location_service
     * \param[in] session The session instance to stop position updates for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_stop_position_updates(
        UALocationServiceSession *session);

    /**
     * \brief Starts heading updates for the supplied session.
     * \ingroup location_service
     * \returns U_STATUS_SUCCESS if the updates were successfully started on the service side, else U_STATUS_ERROR.
     * \param[in] session The session instance to start heading updates for.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_session_start_heading_updates(
        UALocationServiceSession *session);

    /**
     * \brief Stops heading updates for the supplied session.
     * \ingroup location_service
     * \param[in] session The session instance to stop heading updates for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_stop_heading_updates(
        UALocationServiceSession *session);

    /**
     * \brief Starts velocity updates for the supplied session.
     * \ingroup location_service
     * \returns U_STATUS_SUCCESS if the updates were successfully started on the service side, else U_STATUS_ERROR.
     * \param[in] session The session instance to start velocity updates for.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_session_start_velocity_updates(
        UALocationServiceSession *session);

    /**
     * \brief Stops velocity updates for the supplied session.
     * \ingroup location_service
     * \param[in] session The session instance to stop velocity updates for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_session_stop_velocity_updates(
        UALocationServiceSession *session);

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_SESSION_H_
