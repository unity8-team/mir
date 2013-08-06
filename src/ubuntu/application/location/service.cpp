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

#include "ubuntu/application/location/service.h"

UALocationServiceSession*
ua_location_service_create_session_for_low_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return 0;
}

UALocationServiceSession*
ua_location_service_create_session_for_high_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return 0;
}
