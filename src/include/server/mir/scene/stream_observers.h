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

#ifndef MIR_SCENE_STREAM_OBSERVERS_H_
#define MIR_SCENE_STREAM_OBSERVERS_H_

#include "mir/basic_observers.h"
#include "mir/scene/stream_observer.h"

namespace mir
{
namespace scene
{

class StreamObservers : public StreamObserver, BasicObservers<StreamObserver>
{
public:
    using BasicObservers<StreamObserver>::add;
    using BasicObservers<StreamObserver>::remove;
    using BasicObservers<StreamObserver>::for_each;
    void frame_posted(int frames_available) override;
    void resized_to(geometry::Size const& size) override;
};

}
}

#endif /* MIR_SCENE_STREAM_OBSERVERS_H_ */
