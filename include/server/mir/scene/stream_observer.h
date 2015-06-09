/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_SCENE_STREAM_OBSERVER_H_
#define MIR_SCENE_STREAM_OBSERVER_H_

namespace mir
{
namespace geometry
{
struct Size;
}
namespace scene
{
class StreamObserver
{
public:
    virtual void resized_to(geometry::Size const& size) = 0;
    virtual void frame_posted(int frames_available) = 0;

protected:
    StreamObserver() = default;
    virtual ~StreamObserver() = default;
    StreamObserver(StreamObserver const&) = delete;
    StreamObserver& operator=(StreamObserver const&) = delete;
};
}
}

#endif // MIR_SCENE_SURFACE_OBSERVER_H_
