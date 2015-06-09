/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "legacy_stream_change_notification.h"

namespace ms = mir::scene;
namespace geom = mir::geometry;

ms::LegacyStreamChangeNotification::LegacyStreamChangeNotification(
    std::function<void(int)> const& notify_buffer_change) :
    notify_buffer_change(notify_buffer_change)
{
}

void ms::LegacyStreamChangeNotification::frame_posted(int frames_available)
{
    notify_buffer_change(frames_available);
}

void ms::LegacyStreamChangeNotification::resized_to(geom::Size const&)
{
}
