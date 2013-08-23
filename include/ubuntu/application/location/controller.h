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

#ifndef UBUNTU_APPLICATION_LOCATION_SERVICE_CONTROLLER_H_
#define UBUNTU_APPLICATION_LOCATION_SERVICE_CONTROLLER_H_

#include <ubuntu/status.h>
#include <ubuntu/visibility.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * \brief Location service status.
     * \ingroup location_service
     */
    typedef enum
    {
        UA_LOCATION_SERVICE_ENABLED = 1 << 0, /**< The service is enabled. */
        UA_LOCATION_SERVICE_DISABLED = 1 << 1, /**< The service is disabled. */
        UA_LOCATION_SERVICE_GPS_ENABLED = 1 << 2, /**< GPS providers are enabled. */
        UA_LOCATION_SERVICE_GPS_DISABLED = 1 << 3, /**< GPS providers are disabled. */
    } UbuntuApplicationLocationServiceStatus;

    typedef UbuntuApplicationLocationServiceStatus UALocationServiceStatus;

    typedef unsigned int UALocationServiceStatusFlags;

    /**
     * \brief Callback type that is invoked when the location service status changes.
     * \ingroup location_service
     */
    typedef void (*UALocationServiceStatusChangedHandler)(
        UALocationServiceStatusFlags flags,
        void *context);

    /**
     * \brief Opaque type encapsulating a controller the location service.
     * \ingroup location_service
     */
    typedef struct UbuntuApplicationLocationServiceController UALocationServiceController;

    /**
     * \brief Increments the reference count of the controller instance.
     * \ingroup location_service
     * \param[in] controller The controller instance to increment the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_controller_ref(
        UALocationServiceController *controller);

    /**
     * \brief Decrements the reference count of the controller instance.
     * \ingroup location_service
     * \param[in] controller The controller instance to decrement the reference count for.
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_controller_unref(
        UALocationServiceController *controller);

    /**
     * \brief Sets the status changed handler of the controller instance.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     * \param[in] handler The status changed handler to invoke.
     * \param[in] context The context pointer to pass in the status changed handler
     */
    UBUNTU_DLL_PUBLIC void
    ua_location_service_controller_set_status_changed_handler(
        UALocationServiceController *controller,
        UALocationServiceStatusChangedHandler handler,
        void *context);
    
    /**
     * \brief Query the status of the location service.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     * \param[out] flags Flags indicating the service status.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_controller_query_status(
        UALocationServiceController *controller,
        UALocationServiceStatusFlags *out_flags);

    /**
     * \brief Enables the location service.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_controller_enable_service(
        UALocationServiceController *controller);

    /**
     * \brief Disables the location service completely.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_controller_disable_service(
        UALocationServiceController *controller);

    /**
     * \brief Enables all gps providers known to the location service.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_controller_enable_gps(
        UALocationServiceController *controller);

    /**
     * \brief Disables all gps providers known to the location service.
     * \ingroup location_service
     * \param[in] controller The controller instance.
     */
    UBUNTU_DLL_PUBLIC UStatus
    ua_location_service_controller_disable_gps(
        UALocationServiceController *controller);


#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_LOCATION_SERVICE_CONTROLLER_H_
