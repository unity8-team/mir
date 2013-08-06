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

#include "ubuntu/application/location/position_update.h"

void
ua_location_position_update_ref(
    UALocationPositionUpdate *update)
{
    (void) update;
}

void
ua_location_position_update_unref(
    UALocationPositionUpdate *update)
{
    (void) update;
}

uint64_t
ua_location_position_update_get_timestamp(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

double
ua_location_position_update_get_latitude_in_degree(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

double
ua_location_position_update_get_longitude_in_degree(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}

bool
ua_location_position_update_has_altitude(
    UALocationPositionUpdate *update)
{
    (void) update;

    return false;
}

double
ua_location_position_update_get_altitude_in_meter(
    UALocationPositionUpdate *update)
{
    (void) update;

    return 0;
}
