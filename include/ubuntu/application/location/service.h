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

#ifndef UBUNTU_APPLICATION_LOCATION_SERVICE_H_
#define UBUNTU_APPLICATION_LOCATION_SERVICE_H_

#include <ubuntu/visibility.h>

#include <ubuntu/application/location/controller.h>
#include <ubuntu/application/location/session.h>

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * \defgroup location_service Functions and types to provide location services to applications.
     *
     *  Applications in Ubuntu do not access positioning hardware
     *  directly but instead rely on a system-wide location service to
     *  be provided with position, heading and velocity updates. An
     *  application that wants to receive position, heading or
     *  velocity updates needs to start a session with the location
     *  service and specify its requirements with the help
     *  UALocationServiceRequirementsFlags. After the session has been
     *  created, position, heading and velocity updates need to be
     *  started by the application for its session. The actual updates
     *  are delivered via callbacks. Please note that the location
     *  service caches updates as good as possible. That is, if an
     *  application is only interested in a single position fix, and a
     *  reasonable fix is already known to the service, the
     *  information is immediately delivered to the application when
     *  the respecitve updates are started.
     *
     */

    /**
     * \brief Application-specific requirements.
     * \ingroup location_service
     */
    typedef enum
    {
        UA_LOCATION_SERVICE_REQUIRE_ALTITUDE = 1 << 0, /**< The application requires altitude updates. */
        UA_LOCATION_SERVICE_REQUIRE_HEADING = 1 << 1, /**< The application requires heading updates. */
        UA_LOCATION_SERVICE_REQUIRE_VELOCITY = 1 << 2 /**< The application requires velocity updates. */
    } UbuntuApplicationLocationServiceRequirement;

    typedef UbuntuApplicationLocationServiceRequirement UALocationServiceRequirement;

    /**
     * \brief Bitfield type for summarizing an application's requirements.
     * \ingroup location_service
     */
    typedef unsigned int UALocationServiceRequirementsFlags;

    /**
     * \brief Creates a new session with the location service for low positional accuracy requirements.
     * \ingroup location_service
     * \returns A new session or NULL if the requirements cannot be
     * satisfied or if the app lacks permissions to access the
     * location service.
     * \param[in] Bitfield describing the application's requirements.
     */
    UBUNTU_DLL_PUBLIC UALocationServiceSession*
    ua_location_service_create_session_for_low_accuracy(
        UALocationServiceRequirementsFlags flags);

    /**
     * \brief Creates a new session with the location service for high positional accuracy requirements.
     * \ingroup location_service
     * \returns A new session or NULL if the requirements cannot be
     * satisfied or if the app lacks permissions to access the
     * location service.
     * \param[in] Bitfield describing the application's requirements.
     */
    UBUNTU_DLL_PUBLIC UALocationServiceSession*
    ua_location_service_create_session_for_high_accuracy(
        UALocationServiceRequirementsFlags flags);

    /**
     * \brief Creates a new controller for the location service.
     * \ingroup location_service
     * \returns A controller or NULL if the calling process does not
     * have sufficient privileges.
     */
    UBUNTU_DLL_PUBLIC UALocationServiceController*
    ua_location_service_create_controller();

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_SERVICE_H_
