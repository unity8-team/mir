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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_LEGACY_STREAM_CHANGE_NOTIFICATION_H_
#define MIR_SCENE_LEGACY_STREAM_CHANGE_NOTIFICATION_H_

#include "mir/scene/stream_observer.h"

#include <functional>

namespace ms = mir::scene;
namespace geom = mir::geometry;

namespace mir
{
namespace scene
{
class LegacyStreamChangeNotification : public ms::StreamObserver
{
public:
    LegacyStreamChangeNotification(std::function<void(int)> const& stream_change);
    void frame_posted(int frames_available) override;
    void resized_to(geometry::Size const& size) override;
private:
    std::function<void(int)> const notify_buffer_change;
};
}
}

#endif // MIR_SCENE_LEGACY_STREAM_CHANGE_NOTIFICATION_H_
