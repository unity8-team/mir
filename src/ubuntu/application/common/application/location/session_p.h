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
#ifndef SESSION_PRIVATE_H_
#define SESSION_PRIVATE_H_

#include "ubuntu/application/location/session.h"

#include "ref_counted.h"

#include <com/ubuntu/location/service/session/interface.h>

namespace culss = com::ubuntu::location::service::session;

struct UbuntuApplicationLocationServiceSession : public detail::RefCounted
{
    UbuntuApplicationLocationServiceSession(const culss::Interface::Ptr& session)
            : session(session)
    {
    }
    
    culss::Interface::Ptr session;
};

#endif // SESSION_PRIVATE_H_
