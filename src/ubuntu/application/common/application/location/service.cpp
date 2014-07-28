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

#include "controller_p.h"
#include "instance.h"
#include "session_p.h"

#include <com/ubuntu/location/service/stub.h>

#include <core/dbus/resolver.h>
#include <core/dbus/asio/executor.h>

namespace dbus = core::dbus;
namespace cul = com::ubuntu::location;
namespace culs = com::ubuntu::location::service;

UALocationServiceSession*
ua_location_service_create_session_for_low_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    // Creating the instance might fail for a number of reason and
    // we cannot allow exceptions to propagate to prevent applications
    // from aborting. For that, we catch all exceptions, provide some error
    // information to std::cerr and return a nullptr in case of errors.
    try
    {
        return new UbuntuApplicationLocationServiceSession
        {
            // Creating the instance might fail for a number of reasons.

            Instance::instance().get_service()->create_session_for_criteria(cul::Criteria{})
        };
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_create_session_for_low_accuracy: Error creating instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_create_session_for_low_accuracy: Error creating instance." << std::endl;
    }

    return nullptr;
}

UALocationServiceSession*
ua_location_service_create_session_for_high_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    // Creating the instance might fail for a number of reason and
    // we cannot allow exceptions to propagate to prevent applications
    // from aborting. For that, we catch all exceptions, provide some error
    // information to std::cerr and return a nullptr in case of errors.
    try
    {
        return new UbuntuApplicationLocationServiceSession
        {
            // Creating the instance might fail for a number of reasons.

            Instance::instance().get_service()->create_session_for_criteria(cul::Criteria{})
        };
    } catch(const std::exception& e)
    {
        std::cerr << "ua_location_service_create_session_for_high_accuracy: Error creating instance: " << e.what() << std::endl;
    } catch(...)
    {
        std::cerr << "ua_location_service_create_session_for_high_accuracy: Error creating instance." << std::endl;
    }

    return nullptr;
}

UALocationServiceController*
ua_location_service_create_controller()
{
    return new UbuntuApplicationLocationServiceController{};
}
