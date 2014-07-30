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

#include "session_p.h"

#include "heading_update_p.h"
#include "position_update_p.h"
#include "velocity_update_p.h"

namespace location = com::ubuntu::location;

void
ua_location_service_session_ref(
    UALocationServiceSession *session)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    s->ref();
}

void
ua_location_service_session_unref(
    UALocationServiceSession *session)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    s->unref();
}

void
ua_location_service_session_set_position_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionPositionUpdatesHandler handler,
    void *context)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        std::lock_guard<std::mutex> lg(s->position_updates.guard);
        s->position_updates.handler = handler;
        s->position_updates.context = context;
    } catch(const std::exception& e)
    {
        fprintf(stderr, "Error setting up position updates handler: %s \n", e.what());
    } catch(...)
    {
        fprintf(stderr, "Error setting up position updates handler.\n");
    }
}

void
ua_location_service_session_set_heading_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionHeadingUpdatesHandler handler,
    void *context)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        std::lock_guard<std::mutex> lg(s->heading_updates.guard);
        s->heading_updates.handler = handler;
        s->heading_updates.context = context;
    } catch(const std::exception& e)
    {
        fprintf(stderr, "Error setting up heading updates handler: %s \n", e.what());
    } catch(...)
    {
        fprintf(stderr, "Error setting up heading updates handler. \n");
    }
}

void
ua_location_service_session_set_velocity_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionVelocityUpdatesHandler handler,
    void *context)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        std::lock_guard<std::mutex> lg(s->velocity_updates.guard);
        s->velocity_updates.handler = handler;
        s->velocity_updates.context = context;
    } catch(const std::exception& e)
    {
        fprintf(stderr, "Error setting up velocity updates handler: %s \n", e.what());
    } catch(...)
    {
        fprintf(stderr, "Error setting up velocity updates handler.");
    }
}

UStatus
ua_location_service_session_start_position_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return U_STATUS_ERROR;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().position_status.set(
                    location::service::session::Interface::Updates::Status::enabled);
    } catch(...)
    {
        return U_STATUS_ERROR;
    }
    
    return U_STATUS_SUCCESS;
}

void
ua_location_service_session_stop_position_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().position_status.set(
                    location::service::session::Interface::Updates::Status::disabled);
    } catch(...)
    {
    }    
}

UStatus
ua_location_service_session_start_heading_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return U_STATUS_ERROR;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().heading_status.set(
                    location::service::session::Interface::Updates::Status::enabled);
    } catch(...)
    {
        return U_STATUS_ERROR;
    }
    
    return U_STATUS_SUCCESS;
}

void
ua_location_service_session_stop_heading_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().heading_status.set(
                    location::service::session::Interface::Updates::Status::disabled);
    } catch(...)
    {
    }
}

UStatus
ua_location_service_session_start_velocity_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return U_STATUS_ERROR;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().velocity_status.set(
                    location::service::session::Interface::Updates::Status::enabled);
    } catch(...)
    {
        return U_STATUS_ERROR;
    }
    
    return U_STATUS_SUCCESS;
}

void
ua_location_service_session_stop_velocity_updates(
    UALocationServiceSession *session)
{
    if (not session)
        return;

    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);

    try
    {
        s->session->updates().velocity_status.set(
                    location::service::session::Interface::Updates::Status::disabled);
    } catch(...)
    {
    }
}
