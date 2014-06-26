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

#include "heading_update_p.h"
#include "position_update_p.h"
#include "velocity_update_p.h"

#include <com/ubuntu/location/service/session/interface.h>

#include <mutex>

namespace cul = com::ubuntu::location;
namespace culss = com::ubuntu::location::service::session;

struct UbuntuApplicationLocationServiceSession : public detail::RefCounted
{
    UbuntuApplicationLocationServiceSession(const culss::Interface::Ptr& session)
            : session(session),
              connections
              {
                  session->updates().position.changed().connect(
                      [this](const cul::Update<cul::Position>& new_position)
                      {
                          try
                          {
                              std::lock_guard<std::mutex> lg(position_updates.guard);

                              UbuntuApplicationLocationPositionUpdate pu{new_position};
                              if (position_updates.handler) position_updates.handler(
                                  std::addressof(pu),
                                  position_updates.context);
                          } catch(...)
                          {
                              // We silently ignore the issue and keep going.
                          }
                      }),
                  session->updates().heading.changed().connect(
                      [this](const cul::Update<cul::Heading>& new_heading)
                      {
                          try
                          {
                              std::lock_guard<std::mutex> lg(heading_updates.guard);
                              UbuntuApplicationLocationHeadingUpdate hu{new_heading};
                              if (heading_updates.handler) heading_updates.handler(
                                      std::addressof(hu),
                                      heading_updates.context);
                          } catch(...)
                          {
                              // We silently ignore the issue and keep going.
                          }
                      }),
                  session->updates().velocity.changed().connect(
                      [this](const cul::Update<cul::Velocity>& new_velocity)
                      {
                          try
                          {
                              std::lock_guard<std::mutex> lg(velocity_updates.guard);

                              UbuntuApplicationLocationVelocityUpdate vu{new_velocity};
                              if (velocity_updates.handler) velocity_updates.handler(
                                      std::addressof(vu),
                                      velocity_updates.context);
                          } catch(...)
                          {
                              // We silently ignore the issue and keep going.
                          }
                      }),
              }
    {
    }

    ~UbuntuApplicationLocationServiceSession()
    {
        std::lock_guard<std::mutex> lgp(position_updates.guard);
        std::lock_guard<std::mutex> lgh(heading_updates.guard);
        std::lock_guard<std::mutex> lgv(velocity_updates.guard);

        position_updates.handler = nullptr;
        heading_updates.handler = nullptr;
        velocity_updates.handler = nullptr;
    }

    culss::Interface::Ptr session;

    struct
    {
        std::mutex guard;
        UALocationServiceSessionPositionUpdatesHandler handler{nullptr};
        void* context{nullptr};
    } position_updates{};

    struct
    {
        std::mutex guard;
        UALocationServiceSessionHeadingUpdatesHandler handler{nullptr};
        void* context{nullptr};
    } heading_updates{};

    struct
    {
        std::mutex guard;
        UALocationServiceSessionVelocityUpdatesHandler handler{nullptr};
        void* context{nullptr};
    } velocity_updates{};

    struct
    {
        core::ScopedConnection position_updates;
        core::ScopedConnection heading_updates;
        core::ScopedConnection velocity_updates;
    } connections;
};

#endif // SESSION_PRIVATE_H_
