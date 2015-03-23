/*
 * Copyright © 2014-2015 Canonical Ltd.
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
 */

#ifndef MIR_BUFFER_QUEUE_H_
#define MIR_BUFFER_QUEUE_H_

#include "mir/compositor/frame_dropping_policy_factory.h"
#include "mir/compositor/frame_dropping_policy.h"
#include "buffer_bundle.h"

#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>

namespace mir
{

namespace graphics
{
class Buffer;
class GraphicBufferAllocator;
class BufferHandle;
}

namespace compositor
{

class BufferQueue : public BufferBundle,
                    public std::enable_shared_from_this<BufferQueue>
{
public:
    typedef std::function<void(graphics::Buffer* buffer)> Callback;

    BufferQueue(int nbuffers,
                std::shared_ptr<graphics::GraphicBufferAllocator> const& alloc,
                graphics::BufferProperties const& props,
                FrameDroppingPolicyFactory const& policy_provider);

    void client_acquire(Callback complete) override;
    void client_release(graphics::Buffer* buffer) override;
    graphics::BufferHandle compositor_acquire(void const* user_id) override;
    graphics::BufferHandle snapshot_acquire() override;

    graphics::BufferProperties properties() const override;
    void allow_framedropping(bool dropping_allowed) override;
    void force_requests_to_complete() override;
    void resize(const geometry::Size &newsize) override;
    int buffers_ready_for_compositor(void const* user_id) const override;
    int buffers_free_for_client() const override;
    bool framedropping_allowed() const;
    bool is_a_current_buffer_user(void const* user_id) const;
    void drop_old_buffers() override;
    void drop_client_requests() override;

private:
    class LockableCallback;
    enum SnapshotWait
    {
        wait_for_snapshot,
        ignore_snapshot
    };
    void give_buffer_to_client(graphics::Buffer* buffer,
        std::unique_lock<std::mutex>& lock);
    void give_buffer_to_client(graphics::Buffer* buffer,
        std::unique_lock<std::mutex>& lock, SnapshotWait wait_type);
    void release(graphics::Buffer* buffer,
        std::unique_lock<std::mutex> lock);
    void drop_frame(std::unique_lock<std::mutex>& lock, SnapshotWait wait_type);

    mutable std::mutex guard;

    std::vector<std::shared_ptr<graphics::Buffer>> buffers;
    std::deque<graphics::Buffer*> ready_to_composite_queue;
    std::deque<graphics::Buffer*> buffers_owned_by_client;
    std::vector<graphics::Buffer*> free_buffers;
    std::vector<graphics::Buffer*> buffers_sent_to_compositor;
    std::vector<graphics::Buffer*> pending_snapshots;

    std::vector<void const*> current_buffer_users;
    graphics::Buffer* current_compositor_buffer;

    std::deque<Callback> pending_client_notifications;

    int nbuffers;
    bool frame_dropping_enabled;
    bool current_compositor_buffer_valid;
    graphics::BufferProperties the_properties;
    bool force_new_compositor_buffer;
    bool callbacks_allowed;

    std::condition_variable snapshot_released;
    std::shared_ptr<graphics::GraphicBufferAllocator> gralloc;

    // Ensure framedrop_policy gets destroyed first so the callback installed
    // does not access dead objects.
    std::unique_ptr<FrameDroppingPolicy> framedrop_policy;
};

}
}

#endif
