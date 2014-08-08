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

#include "ubuntu/application/location/controller.h"

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

    void set_changed_handler_with_context(
            UALocationServiceStatusChangedHandler handler,
            void* context)
    {
        changed_handler = handler;
        changed_handler_context = context;
    }

  private:
    Instance()
        : bus(std::make_shared<core::dbus::Bus>(core::dbus::WellKnownBus::system)),
          executor(core::dbus::asio::make_executor(bus)),
          service(core::dbus::resolve_service_on_bus<
                    com::ubuntu::location::service::Interface,
                    com::ubuntu::location::service::Stub
                  >(bus)),
          connections
          {
              service->does_satellite_based_positioning().changed().connect([this](bool value)
              {
                  // Update our cached value.
                  if (value)
                      cached_state_flags |= UA_LOCATION_SERVICE_GPS_ENABLED;
                  else
                      cached_state_flags |= UA_LOCATION_SERVICE_GPS_DISABLED;

                  // And notify change handler if one is set.
                  if (changed_handler)
                      changed_handler(cached_state_flags, changed_handler_context);
              }),
              service->is_online().changed().connect([this](bool value)
              {
                  // Update our cached value.
                  if (value)
                      cached_state_flags |= UA_LOCATION_SERVICE_ENABLED;
                  else
                      cached_state_flags |= UA_LOCATION_SERVICE_DISABLED;

                  // And notify change handler if one is set.
                  if (changed_handler)
                      changed_handler(cached_state_flags, changed_handler_context);
              })
          },
          cached_state_flags{0},
          changed_handler{nullptr},
          changed_handler_context{nullptr}
    {
        bus->install_executor(executor);
        worker = std::move(std::thread([&]() { bus->run(); }));
    }

    ~Instance() noexcept
    {
        try
        {
            bus->stop();

            if (worker.joinable())
                worker.join();
        } catch(...)
        {
            // We silently ignore errors to fulfill our noexcept guarantee.
        }
    }

    core::dbus::Bus::Ptr bus;
    core::dbus::Executor::Ptr executor;
    std::thread worker;

    com::ubuntu::location::service::Interface::Ptr service;

    // All event connections go here.
    struct
    {
        core::ScopedConnection on_does_satellite_based_positioning_changed;
        core::ScopedConnection on_is_online_changed;
    } connections;

    // All change-handler specifics go here.
    UALocationServiceStatusFlags cached_state_flags;
    UALocationServiceStatusChangedHandler changed_handler;
    void* changed_handler_context;
};

#endif // INSTANCE_H_
