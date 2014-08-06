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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "ubuntu/application/location/controller.h"

#include "controller_p.h"
#include "instance.h"

void
ua_location_service_controller_ref(
    UALocationServiceController *controller)
{
    controller->ref();
}

void
ua_location_service_controller_unref(
    UALocationServiceController *controller)
{
    controller->unref();
}

void
ua_location_service_controller_set_status_changed_handler(
    UALocationServiceController *controller,
    UALocationServiceStatusChangedHandler handler,
    void *context)
{
    (void) controller;

    try
    {
        Instance::instance().set_changed_handler_with_context(handler, context);
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_set_status_changed_handler: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_set_status_changed_handler: error accessing instance." << std::endl;
    }
}

UStatus
ua_location_service_controller_query_status(
    UALocationServiceController *controller,
    UALocationServiceStatusFlags *out_flags)
{
    (void) controller;

    *out_flags = 0;

    try
    {
        auto service = Instance::instance().get_service();

        if (service->is_online().get())
            *out_flags |= UA_LOCATION_SERVICE_ENABLED;
        else
            *out_flags |= UA_LOCATION_SERVICE_DISABLED;

        if (service->does_satellite_based_positioning().get())
            *out_flags |= UA_LOCATION_SERVICE_GPS_ENABLED;
        else
            *out_flags |= UA_LOCATION_SERVICE_GPS_DISABLED;

        return U_STATUS_SUCCESS;
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_query_status: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_query_status: error accessing instance." << std::endl;
    }

    return U_STATUS_ERROR;
}

UStatus
ua_location_service_controller_enable_service(
    UALocationServiceController *controller)
{
    (void) controller;

    try
    {
        auto service = Instance::instance().get_service();
        service->is_online().set(true);

        return U_STATUS_SUCCESS;
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_enable_service: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_enable_service: error accessing instance." << std::endl;
    }

    return U_STATUS_ERROR;
}

UStatus
ua_location_service_controller_disable_service(
    UALocationServiceController *controller)
{
    (void) controller;

    try
    {
        auto service = Instance::instance().get_service();
        service->is_online().set(false);

        return U_STATUS_SUCCESS;
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_disable_service: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_disable_service: error accessing instance." << std::endl;
    }

    return U_STATUS_ERROR;
}

UStatus
ua_location_service_controller_enable_gps(
    UALocationServiceController *controller)
{
    (void) controller;

    try
    {
        auto service = Instance::instance().get_service();
        service->does_satellite_based_positioning().set(true);

        return U_STATUS_SUCCESS;
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_enable_gps: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_enable_gps: error accessing instance." << std::endl;
    }

    return U_STATUS_ERROR;
}

UStatus
ua_location_service_controller_disable_gps(
    UALocationServiceController *controller)
{
    (void) controller;

    try
    {
        auto service = Instance::instance().get_service();
        service->does_satellite_based_positioning().set(false);

        return U_STATUS_SUCCESS;
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_controller_disable_gps: error accessing instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_controller_disable_gps: error accessing instance." << std::endl;
    }

    return U_STATUS_ERROR;
}
