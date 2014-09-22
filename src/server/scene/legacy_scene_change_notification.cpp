/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "legacy_surface_change_notification.h"

#include "mir/scene/legacy_scene_change_notification.h"
#include "mir/scene/surface.h"

#include <boost/throw_exception.hpp>

namespace ms = mir::scene;

ms::LegacySceneChangeNotification::LegacySceneChangeNotification(
    std::function<void()> const& scene_notify_change,
    std::function<void(int)> const& buffer_notify_change)
    : scene_notify_change(scene_notify_change),
      buffer_notify_change(buffer_notify_change)
      
{
}

ms::LegacySceneChangeNotification::~LegacySceneChangeNotification()
{
    end_observation();
}

void ms::LegacySceneChangeNotification::add_surface_observer(ms::Surface* surface)
{
    auto observer = std::make_shared<ms::LegacySurfaceChangeNotification>(
        scene_notify_change, buffer_notify_change);
    surface->add_observer(observer);
    
    {
        std::unique_lock<decltype(surface_observers_guard)> lg(surface_observers_guard);
        surface_observers[surface] = observer;
    }
    
    scene_notify_change();
}

void ms::LegacySceneChangeNotification::surface_added(ms::Surface* surface)
{
    add_surface_observer(surface);
}

void ms::LegacySceneChangeNotification::surface_exists(ms::Surface* surface)
{
    add_surface_observer(surface);
}
    
void ms::LegacySceneChangeNotification::surface_removed(ms::Surface* surface)
{
    {
        std::unique_lock<decltype(surface_observers_guard)> lg(surface_observers_guard);
        auto it = surface_observers.find(surface);
        if (it != surface_observers.end())
        {
            surface->remove_observer(it->second);
            surface_observers.erase(it);
        }
    }
    scene_notify_change();
}

void ms::LegacySceneChangeNotification::surfaces_reordered()
{
    scene_notify_change();
}

void ms::LegacySceneChangeNotification::scene_changed()
{
    scene_notify_change();
}

void ms::LegacySceneChangeNotification::end_observation()
{
    std::unique_lock<decltype(surface_observers_guard)> lg(surface_observers_guard);
    for (auto &kv : surface_observers)
    {
        auto surface = kv.first;
        if (surface)
            surface->remove_observer(kv.second);
    }
    surface_observers.clear();
}
