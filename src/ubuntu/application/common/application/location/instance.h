/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef INSTANCE_H_
#define INSTANCE_H_

#include "ubuntu/visibility.h"

#include <com/ubuntu/location/service/stub.h>

#include <core/dbus/resolver.h>
#include <core/dbus/asio/executor.h>

#include <thread>

class UBUNTU_DLL_LOCAL Instance
{
  public:
    static Instance& instance()
    {
        static Instance inst;
        return inst;
    }

    const com::ubuntu::location::service::Interface::Ptr& get_service() const
    {
        return service;
    }

  private:
    Instance() : bus(std::make_shared<core::dbus::Bus>(core::dbus::WellKnownBus::system)),
                 executor(core::dbus::asio::make_executor(bus)),
                 service(core::dbus::resolve_service_on_bus<
                            com::ubuntu::location::service::Interface,
                            com::ubuntu::location::service::Stub
                         >(bus))
    {
        bus->install_executor(executor);
        worker = std::move(std::thread([&]() { bus->run(); }));
    }

    ~Instance() noexcept
    {
        bus->stop();

        if (worker.joinable())
            worker.join();
    }

    core::dbus::Bus::Ptr bus;
    core::dbus::Executor::Ptr executor;
    com::ubuntu::location::service::Interface::Ptr service;
    std::thread worker;
};

#endif // INSTANCE_H_
