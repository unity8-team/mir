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

#include "ubuntu/application/location/heading_update.h"

#include "heading_update_p.h"

#include <com/ubuntu/location/heading.h>
#include <com/ubuntu/location/update.h>

namespace cul = com::ubuntu::location;

void
ua_location_heading_update_ref(
    UALocationHeadingUpdate *update)
{
    update->ref();
}

void
ua_location_heading_update_unref(
    UALocationHeadingUpdate *update)
{
    update->unref();
}

uint64_t
ua_location_heading_update_get_timestamp(
    UALocationHeadingUpdate *update)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        update->update.when.time_since_epoch()).count();
}

double
ua_location_heading_update_get_heading_in_degree(
    UALocationHeadingUpdate *update)
{
    return update->update.value.value();
}
