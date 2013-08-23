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

#include "session_p.h"

#include <com/ubuntu/location/service/stub.h>
#include <org/freedesktop/dbus/resolver.h>
#include <org/freedesktop/dbus/asio/executor.h>

namespace dbus = org::freedesktop::dbus;
namespace cul = com::ubuntu::location;
namespace culs = com::ubuntu::location::service;

namespace
{
class Instance
{
  public:
    static Instance& instance()
    {
        static Instance inst;
        return inst;
    }

    const culs::Interface::Ptr& get_service() const
    {
        return service;
    }

  private:
    Instance() : bus(std::make_shared<dbus::Bus>(dbus::WellKnownBus::system)),
                 executor(std::make_shared<dbus::asio::Executor>(bus)),
                 service(dbus::resolve_service_on_bus<culs::Interface, culs::Stub>(bus))
    {
        bus->install_executor(executor);
        worker = std::move(std::thread([&]() { bus->run(); }));
    }

    ~Instance() noexcept
    {
        if (worker.joinable())
            worker.join();
    }

    dbus::Bus::Ptr bus;
    dbus::Executor::Ptr executor;
    culs::Interface::Ptr service;        
    std::thread worker;
};

}

UALocationServiceSession*
ua_location_service_create_session_for_low_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return new UbuntuApplicationLocationServiceSession{
        Instance::instance().get_service()->create_session_for_criteria(cul::Criteria{})};
}

UALocationServiceSession*
ua_location_service_create_session_for_high_accuracy(
    UALocationServiceRequirementsFlags /*flags*/)
{
    return new UbuntuApplicationLocationServiceSession{
        Instance::instance().get_service()->create_session_for_criteria(cul::Criteria{})};
}
