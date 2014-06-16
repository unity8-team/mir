/*
 * Copyright © 2014 Canonical Ltd.
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

#include <ubuntu/application/location/service.h>
#include <ubuntu/application/location/session.h>

#include <ubuntu/application/location/position_update.h>
#include <ubuntu/application/location/heading_update.h>
#include <ubuntu/application/location/velocity_update.h>

#include <signal.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

namespace
{
void on_position_updated(UALocationPositionUpdate* update, void*)
{
    printf("%s@%" PRIu64 ": (%f, %f, %f) \n",
           __PRETTY_FUNCTION__,
           ua_location_position_update_get_timestamp(update),
           ua_location_position_update_get_latitude_in_degree(update),
           ua_location_position_update_get_longitude_in_degree(update),
           ua_location_position_update_has_altitude(update) ? ua_location_position_update_get_altitude_in_meter(update) : 0);
}

void on_heading_updated(UALocationHeadingUpdate* update, void*)
{
    printf("%s@%" PRIu64 ": %f \n",
           __PRETTY_FUNCTION__,
           ua_location_heading_update_get_timestamp(update),
           ua_location_heading_update_get_heading_in_degree(update));
}

void on_velocity_updated(UALocationVelocityUpdate* update, void*)
{
    printf("%s@%" PRIu64 ": %f \n",
           __PRETTY_FUNCTION__,
           ua_location_velocity_update_get_timestamp(update),
           ua_location_velocity_update_get_velocity_in_meters_per_second(update));
}

void wait_for_sigint()
{
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);

    int sig;
    int result = sigwait(&signals, &sig);
    if (result != 0)
        printf("sigwait failed!\n");
}
}

int main(int argc, char** argv)
{
    UALocationServiceSession* session =
            ua_location_service_create_session_for_high_accuracy(0);
    
    ua_location_service_session_set_position_updates_handler(
        session,
        on_position_updated,
        NULL);

    ua_location_service_session_set_heading_updates_handler(
        session,
        on_heading_updated,
        NULL);

    ua_location_service_session_set_velocity_updates_handler(
        session,
        on_velocity_updated,
        NULL);

    ua_location_service_session_start_position_updates(session);
    ua_location_service_session_start_heading_updates(session);
    ua_location_service_session_start_velocity_updates(session);

    wait_for_sigint();
    
    ua_location_service_session_stop_position_updates(session);
    ua_location_service_session_stop_heading_updates(session);
    ua_location_service_session_stop_velocity_updates(session);

    ua_location_service_session_unref(session);

    return 0;
}
