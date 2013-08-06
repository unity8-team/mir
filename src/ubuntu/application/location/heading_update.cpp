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

void
ua_location_heading_update_ref(
    UALocationHeadingUpdate *update)
{
    (void) update;
}

void
ua_location_heading_update_unref(
    UALocationHeadingUpdate *update)
{
    (void) update;
}

uint64_t
ua_location_heading_update_get_timestamp(
    UALocationHeadingUpdate *update)
{
    (void) update;
    return 0;
}

double
ua_location_position_update_get_heading_in_degree(
    UALocationHeadingUpdate *update)
{
    (void) update;
    
    return 0;
}
