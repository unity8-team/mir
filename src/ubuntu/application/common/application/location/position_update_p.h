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

#ifndef POSITION_UPDATE_PRIVATE_H_
#define POSITION_UPDATE_PRIVATE_H_

#include "ubuntu/application/location/position_update.h"

#include "ref_counted.h"

#include <com/ubuntu/location/position.h>
#include <com/ubuntu/location/update.h>

namespace cul = com::ubuntu::location;

struct UbuntuApplicationLocationPositionUpdate : public detail::RefCounted
{
    UbuntuApplicationLocationPositionUpdate(const cul::Update<cul::Position>& update) : update(update)
    {
    }

    const cul::Update<cul::Position>& update;
};

#endif // POSITION_UPDATE_PRIVATE_H_
