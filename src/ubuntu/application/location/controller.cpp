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
    (void) handler;
    (void) context;
}

UStatus
ua_location_service_controller_query_status(
    UALocationServiceController *controller,
    UALocationServiceStatusFlags *out_flags)
{
    (void) controller;
    (void) out_flags;

    return U_STATUS_SUCCESS;
}

UStatus
ua_location_service_controller_enable_service(
    UALocationServiceController *controller)
{
    (void) controller;

    return U_STATUS_SUCCESS;
}

UStatus
ua_location_service_controller_disable_service(
    UALocationServiceController *controller)
{
    (void) controller;
    
    return U_STATUS_SUCCESS;
}

UStatus
ua_location_service_controller_enable_gps(
    UALocationServiceController *controller)
{
    (void) controller;

    return U_STATUS_SUCCESS;
}

UStatus
ua_location_service_controller_disable_gps(
    UALocationServiceController *controller)
{
    (void) controller;

    return U_STATUS_SUCCESS;
}
