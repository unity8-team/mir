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

#include "position_update_p.h"

#include <com/ubuntu/location/position.h>
#include <com/ubuntu/location/update.h>

namespace cul = com::ubuntu::location;

void
ua_location_position_update_ref(
    UALocationPositionUpdate *update)
{
    update->ref();
}

void
ua_location_position_update_unref(
    UALocationPositionUpdate *update)
{
    update->unref();
}

uint64_t
ua_location_position_update_get_timestamp(
    UALocationPositionUpdate *update)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        update->update.when.time_since_epoch()).count();
}

double
ua_location_position_update_get_latitude_in_degree(
    UALocationPositionUpdate *update)
{
    return update->update.value.latitude.value.value();
}

double
ua_location_position_update_get_longitude_in_degree(
    UALocationPositionUpdate *update)
{
    return update->update.value.longitude.value.value();
}

bool
ua_location_position_update_has_altitude(
    UALocationPositionUpdate *update)
{
    return update->update.value.altitude;
}

double
ua_location_position_update_get_altitude_in_meter(
    UALocationPositionUpdate *update)
{
    return update->update.value.altitude->value.value();
}

bool
ua_location_position_update_has_horizontal_accuracy(
    UALocationPositionUpdate *update)
{
    return update->update.value.accuracy.horizontal;
}

double
ua_location_position_update_get_horizontal_accuracy_in_meter(
    UALocationPositionUpdate *update)
{
    return update->update.value.accuracy.horizontal->value();
}

bool
ua_location_position_update_has_vertical_accuracy(
    UALocationPositionUpdate *update)
{
    return update->update.value.accuracy.vertical;
}

double
ua_location_position_update_get_vertical_accuracy_in_meter(
    UALocationPositionUpdate *update)
{
    return update->update.value.accuracy.vertical->value();
}
