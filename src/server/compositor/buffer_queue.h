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
}
namespace compositor
{

enum class BufferAllocBehavior
{
    /* The queue will allocate all buffers at construction time */
    allocate_all,
    /* The queue will shrink and grow depending on client and compositor demand */
    on_demand
};

struct BufferAllocationPolicy
{
    BufferAllocationPolicy()
        : alloc_behavior {BufferAllocBehavior::on_demand},
          shrink_treshold{300}
    {
    }

    BufferAllocationPolicy(BufferAllocBehavior behavior, int treshold)
        : alloc_behavior{behavior}, shrink_treshold{treshold}
    {
    }

    BufferAllocBehavior alloc_behavior;
    /* The shrink threshold is ignored if alloc_behavior is allocate_all
     * With on_demand behavior it specifies when to shrink the queue after it has expanded.
     * It signifies the number of times the queue must be found in excess of buffers
     * before it shrinks. This is used to avoid rapid growth and shrink behavior.
     */
    int const shrink_treshold;
};

class BufferQueue : public BufferBundle
{
public:
    typedef std::function<void(graphics::Buffer* buffer)> Callback;

    BufferQueue(int nbuffers,
                std::shared_ptr<graphics::GraphicBufferAllocator> const& alloc,
                graphics::BufferProperties const& props,
                FrameDroppingPolicyFactory const& policy_provider,
                BufferAllocationPolicy const& alloc_policy);

    void client_acquire(Callback complete) override;
    void client_release(graphics::Buffer* buffer) override;
    std::shared_ptr<graphics::Buffer> compositor_acquire(void const* user_id) override;
    void compositor_release(std::shared_ptr<graphics::Buffer> const& buffer) override;
    std::shared_ptr<graphics::Buffer> snapshot_acquire() override;
    void snapshot_release(std::shared_ptr<graphics::Buffer> const& buffer) override;

    graphics::BufferProperties properties() const override;
    void allow_framedropping(bool dropping_allowed) override;
    void force_requests_to_complete() override;
    void resize(const geometry::Size &newsize) override;
    int buffers_ready_for_compositor() const override;
    int buffers_free_for_client() const override;

    bool framedropping_allowed() const;
    int allocated_buffers() const;

private:
    void give_buffer_to_client(graphics::Buffer* buffer,
        std::unique_lock<std::mutex> lock);
    bool is_a_current_buffer_user(void const* user_id) const;
    void release(graphics::Buffer* buffer,
        std::unique_lock<std::mutex> lock);
    void drop_frame(std::unique_lock<std::mutex> lock);
    int min_buffers() const;

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
    int excess;
    BufferAllocationPolicy const alloc_policy;
    bool overlapping_compositors;
    bool frame_dropping_enabled;
    std::unique_ptr<FrameDroppingPolicy> framedrop_policy;
    graphics::BufferProperties the_properties;

    std::condition_variable snapshot_released;
    std::shared_ptr<graphics::GraphicBufferAllocator> gralloc;
};

}
}

#endif
