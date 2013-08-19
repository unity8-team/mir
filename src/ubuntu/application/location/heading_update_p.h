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

#ifndef HEADING_UPDATE_PRIVATE_H_
#define HEADING_UPDATE_PRIVATE_H_

#include "ubuntu/application/location/heading_update.h"

#include "ref_counted.h"

#include <com/ubuntu/location/heading.h>
#include <com/ubuntu/location/update.h>

namespace cul = com::ubuntu::location;

struct UbuntuApplicationLocationHeadingUpdate : public detail::RefCounted
{
    UbuntuApplicationLocationHeadingUpdate(const cul::Update<cul::Heading>& update) : update(update)
    {
    }

    const cul::Update<cul::Heading>& update;
};

#endif // HEADING_UPDATE_PRIVATE_H_
