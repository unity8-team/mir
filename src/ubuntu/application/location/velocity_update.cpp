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

#include "ubuntu/application/location/velocity_update.h"

#include "velocity_update_p.h"

#include <com/ubuntu/location/update.h>
#include <com/ubuntu/location/velocity.h>

namespace cul = com::ubuntu::location;

void
ua_location_velocity_update_ref(
    UALocationVelocityUpdate *update)
{
    (void) update;
}

void
ua_location_velocity_update_unref(
    UALocationVelocityUpdate *update)
{
    (void) update;
}

uint64_t
ua_location_velocity_update_get_timestamp(
    UALocationVelocityUpdate *update)
{
    auto u = static_cast<UbuntuApplicationLocationVelocityUpdate*>(update);

    if (!u)
        return 0;

    return std::chrono::duration_cast<std::chrono::microseconds>(
        u->update.when.time_since_epoch()).count();
}

double
ua_location_velocity_update_get_velocity_in_meters_per_second(
    UALocationVelocityUpdate *update)
{
    auto u = static_cast<UbuntuApplicationLocationVelocityUpdate*>(update);

    if (!u)
        return 0;

    return u->update.value.value.value();
}
