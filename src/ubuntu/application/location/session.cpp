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

#include "ubuntu/application/location/session.h"

void
ua_location_service_session_ref(
    UALocationServiceSession *session)
{
    (void) session;
}

void
ua_location_service_session_unref(
    UALocationServiceSession *session)
{
    (void) session;
}

void
ua_location_service_session_set_position_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionPositionUpdatesHandler handler,
    void *context)
{
    (void) session;
    (void) handler;
    (void) context;
}

void
ua_location_service_session_set_heading_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionHeadingUpdatesHandler handler,
    void *context)
{
    (void) session;
    (void) handler;
    (void) context;
}

void
ua_location_service_session_set_velocity_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionVelocityUpdatesHandler handler,
    void *context)
{
    (void) session;
    (void) handler;
    (void) context;
}

UStatus
ua_location_service_session_start_position_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_position_updates(
    UALocationServiceSession *session)
{
    (void) session;
}

UStatus
ua_location_service_session_start_heading_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_heading_updates(
    UALocationServiceSession *session)
{
    (void) session;
}

UStatus
ua_location_service_session_start_velocity_updates(
    UALocationServiceSession *session)
{
    (void) session;
    return U_STATUS_ERROR;
}

void
ua_location_service_session_stop_velocity_updates(
    UALocationServiceSession *session)
{
    (void) session;
}
