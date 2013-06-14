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

#ifndef MIR_COMPOSITOR_BUFFER_SWAPPER_MULTI_H_
#define MIR_COMPOSITOR_BUFFER_SWAPPER_MULTI_H_

#include "buffer_swapper.h"

#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <deque>

namespace mir
{
namespace compositor
{
namespace detail
{

struct AcquiredBufferInfo
{
    std::weak_ptr<Buffer> buffer;
    bool can_be_reacquired;
    int use_count;
};

class AcquiredBuffers
{
public:
    void acquire(std::shared_ptr<Buffer> const& buffer);
    std::pair<bool,bool> release(std::shared_ptr<Buffer> const& buffer);
    std::shared_ptr<Buffer> find_reacquirable_buffer();
    std::shared_ptr<Buffer> last_buffer();

private:
    std::vector<AcquiredBufferInfo>::iterator find_info(std::shared_ptr<Buffer> const& buffer);

    std::vector<AcquiredBufferInfo> acquired_buffers;
};

}

class Buffer;

class BufferSwapperMulti : public BufferSwapper
{
public:
    explicit BufferSwapperMulti(std::vector<std::shared_ptr<compositor::Buffer>>& buffer_list, size_t swapper_size);

    std::shared_ptr<Buffer> client_acquire();
    void client_release(std::shared_ptr<Buffer> const& queued_buffer);
    std::shared_ptr<Buffer> compositor_acquire();
    void compositor_release(std::shared_ptr<Buffer> const& released_buffer);

    void force_client_abort();
    void force_requests_to_complete();
    void end_responsibility(std::vector<std::shared_ptr<Buffer>>&, size_t&);

private:
    std::mutex swapper_mutex;
    std::condition_variable client_available_cv;

    std::deque<std::shared_ptr<Buffer>> client_queue;
    std::deque<std::shared_ptr<Buffer>> compositor_queue;
    detail::AcquiredBuffers acquired_buffers;
    unsigned int in_use_by_client;
    unsigned int const swapper_size;
    int clients_trying_to_acquire;
    bool force_clients_to_complete;
};

}
}

#endif /* MIR_COMPOSITOR_BUFFER_SWAPPER_MULTI_H_ */
