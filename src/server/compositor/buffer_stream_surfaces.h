/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by:
 * Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_COMPOSITOR_BUFFER_STREAM_SCENE_H_
#define MIR_COMPOSITOR_BUFFER_STREAM_SCENE_H_

#include "mir/compositor/buffer_stream.h"
#include "mir/scene/stream_observers.h"

#include <mutex>

namespace mir
{
namespace compositor
{

class BufferIDUniqueGenerator;
class BufferBundle;
class BackBufferStrategy;

class BufferStreamSurfaces : public BufferStream
{
public:
    BufferStreamSurfaces(std::shared_ptr<BufferBundle> const& swapper);
    ~BufferStreamSurfaces();

    //from mf::BufferStream
    void swap_buffers(
        graphics::Buffer* old_buffer, std::function<void(graphics::Buffer* new_buffer)> complete) override;
    void with_most_recent_buffer_do(std::function<void(graphics::Buffer&)> const& exec) override;
    MirPixelFormat pixel_format() const override;
    void add_observer(std::shared_ptr<scene::StreamObserver> const& observer) override;
    void remove_observer(std::weak_ptr<scene::StreamObserver> const& observer) override;

    //from mc::BufferStream
    void acquire_client_buffer(std::function<void(graphics::Buffer* buffer)> complete);
    void release_client_buffer(graphics::Buffer* buf);

    std::shared_ptr<graphics::Buffer>
        lock_compositor_buffer(void const* user_id) override;

    geometry::Size stream_size() override;
    void resize(geometry::Size const& size) override;
    void allow_framedropping(bool) override;
    void force_requests_to_complete() override;
    int buffers_ready_for_compositor(void const* user_id) const override;
    void drop_old_buffers() override;
    bool has_submitted_buffer() const override;

protected:
    BufferStreamSurfaces(const BufferStreamSurfaces&) = delete;
    BufferStreamSurfaces& operator=(const BufferStreamSurfaces&) = delete;

private:
    std::mutex mutable mutex;
    std::shared_ptr<BufferBundle> const buffer_bundle;
    scene::StreamObservers observers;
    bool first_frame_posted;
};

}
}

#endif /* MIR_COMPOSITOR_BUFFER_STREAM_SCENE_H_ */
