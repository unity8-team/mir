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

void
ua_location_service_session_ref(
    UALocationServiceSession *session)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
}

void
ua_location_service_session_unref(
    UALocationServiceSession *session)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
}

void
ua_location_service_session_set_position_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionPositionUpdatesHandler handler,
    void *context)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    s->session->install_position_updates_handler(
        [handler, context](const com::ubuntu::location::Update<com::ubuntu::location::Position>& new_position)
        {
            UbuntuApplicationLocationPositionUpdate pu{new_position};
            handler(std::addressof(pu), context);
        });
}

void
ua_location_service_session_set_heading_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionHeadingUpdatesHandler handler,
    void *context)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    s->session->install_heading_updates_handler(
        [handler, context](const com::ubuntu::location::Update<com::ubuntu::location::Heading>& new_heading) 
        {
            UbuntuApplicationLocationHeadingUpdate hu{new_heading};
            handler(std::addressof(hu), context);
        });
}

void
ua_location_service_session_set_velocity_updates_handler(
    UALocationServiceSession *session,
    UALocationServiceSessionVelocityUpdatesHandler handler,
    void *context)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    s->session->install_velocity_updates_handler(
        [handler, context](const com::ubuntu::location::Update<com::ubuntu::location::Velocity>& new_velocity) 
        {
            UbuntuApplicationLocationVelocityUpdate vu{new_velocity};
            handler(std::addressof(vu), context);
        });
}

UStatus
ua_location_service_session_start_position_updates(
    UALocationServiceSession *session)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return U_STATUS_ERROR;

    try
    {
        s->session->start_position_updates();
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
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return;

    try
    {
        s->session->stop_position_updates();
    } catch(...)
    {
    }    
}

UStatus
ua_location_service_session_start_heading_updates(
    UALocationServiceSession *session)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return U_STATUS_ERROR;

    try
    {
        s->session->start_heading_updates();
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
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return;

    try
    {
        s->session->stop_heading_updates();
    } catch(...)
    {
    }
}

UStatus
ua_location_service_session_start_velocity_updates(
    UALocationServiceSession *session)
{
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return U_STATUS_ERROR;

    try
    {
        s->session->start_velocity_updates();
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
    auto s = static_cast<UbuntuApplicationLocationServiceSession*>(session);
    if (!s)
        return;

    try
    {
        s->session->stop_velocity_updates();
    } catch(...)
    {
    }
}
