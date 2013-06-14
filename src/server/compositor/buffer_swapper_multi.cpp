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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/compositor/buffer_swapper_multi.h"
#include <boost/throw_exception.hpp>
#include <algorithm>

namespace mc = mir::compositor;

std::vector<mc::detail::AcquiredBufferInfo>::iterator
mc::detail::AcquiredBuffers::find_info(std::shared_ptr<Buffer> const& buffer)
{
    return std::find_if(acquired_buffers.begin(), acquired_buffers.end(),
                        [&buffer] (AcquiredBufferInfo const& info)
                        {
                            return info.buffer.lock() == buffer;
                        });
}

void mc::detail::AcquiredBuffers::acquire(std::shared_ptr<Buffer> const& buffer)
{
    auto iter = find_info(buffer);

    if (iter != acquired_buffers.end())
        ++iter->use_count;
    else
        acquired_buffers.push_back(AcquiredBufferInfo{buffer, true, 1});

}

std::pair<bool,bool>
mc::detail::AcquiredBuffers::release(std::shared_ptr<Buffer> const& buffer)
{
    auto iter = find_info(buffer);

    bool released{false};
    bool found{false};

    if (iter != acquired_buffers.end())
    {
        AcquiredBufferInfo& info = *iter;
        found = true;

        --info.use_count;

        if (info.use_count == 0)
        {
            acquired_buffers.erase(iter);
            released = true;
        }
        else
        {
            info.can_be_reacquired = false;
        }
    }

    return std::make_pair(released, found);
}

std::shared_ptr<mc::Buffer> mc::detail::AcquiredBuffers::find_reacquirable_buffer()
{
    auto iter = std::find_if(acquired_buffers.begin(), acquired_buffers.end(),
                    [] (AcquiredBufferInfo const& info)
                    {
                        return info.can_be_reacquired == true;
                    });

    if (iter != acquired_buffers.end())
    {
        return iter->buffer.lock();
    }
    else
    {
        return {};
    }
}

std::shared_ptr<mc::Buffer> mc::detail::AcquiredBuffers::last_buffer()
{
    if (!acquired_buffers.empty())
    {
        return acquired_buffers.back().buffer.lock();
    }
    else
    {
        return {};
    }
}

mc::BufferSwapperMulti::BufferSwapperMulti(std::vector<std::shared_ptr<compositor::Buffer>>& buffer_list, size_t swapper_size)
 : in_use_by_client(0),
   swapper_size(swapper_size),
   clients_trying_to_acquire(0),
   force_clients_to_complete(false)
{
    if ((swapper_size != 2) && (swapper_size != 3))
    {
        BOOST_THROW_EXCEPTION(std::logic_error("BufferSwapperMulti is only validated for 2 or 3 buffers"));
    }

    for (auto& buffer : buffer_list)
    {
        client_queue.push_back(buffer);
    }
}

std::shared_ptr<mc::Buffer> mc::BufferSwapperMulti::client_acquire()
{
    std::unique_lock<std::mutex> lk(swapper_mutex);

    clients_trying_to_acquire++;

    /*
     * Don't allow the client to acquire all the buffers, because then the
     * compositor won't have a buffer to display.
     */
    while ((!force_clients_to_complete) &&
           (client_queue.empty() || (in_use_by_client == (swapper_size - 1))))
    {
        client_available_cv.wait(lk);
    }

    //we have been forced to shutdown
    if (force_clients_to_complete)
    {
        clients_trying_to_acquire--;
        BOOST_THROW_EXCEPTION(std::logic_error("forced_completion"));
    }

    auto dequeued_buffer = client_queue.front();
    client_queue.pop_front();
    in_use_by_client++;

    clients_trying_to_acquire--;

    return dequeued_buffer;
}

void mc::BufferSwapperMulti::client_release(std::shared_ptr<Buffer> const& queued_buffer)
{
    std::unique_lock<std::mutex> lk(swapper_mutex);

    compositor_queue.push_back(queued_buffer);
    in_use_by_client--;

    /*
     * At this point we could signal the client_available_cv.  However, we
     * won't do so, because the cv will get signaled after the next
     * compositor_release anyway, and we want to avoid the overhead of
     * signaling at every client_release just to ensure quicker client
     * notification in an abnormal situation.
     */
}

std::shared_ptr<mc::Buffer> mc::BufferSwapperMulti::compositor_acquire()
{
    std::unique_lock<std::mutex> lk(swapper_mutex);

    std::shared_ptr<mc::Buffer> dequeued_buffer;

    if ((dequeued_buffer = acquired_buffers.find_reacquirable_buffer()) != nullptr)
    {
    }
    else if (!compositor_queue.empty())
    {
        dequeued_buffer = compositor_queue.front();
        compositor_queue.pop_front();
    }
    else if (!client_queue.empty())
    {
        dequeued_buffer = client_queue.back();
        client_queue.pop_back();
    }
    else if ((dequeued_buffer = acquired_buffers.last_buffer()) != nullptr)
    {
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::logic_error("forced_completion"));
    }

    acquired_buffers.acquire(dequeued_buffer);

    return dequeued_buffer;
}

void mc::BufferSwapperMulti::compositor_release(std::shared_ptr<Buffer> const& released_buffer)
{
    std::unique_lock<std::mutex> lk(swapper_mutex);

    auto const rel_pair = acquired_buffers.release(released_buffer);

    bool const buffer_released{rel_pair.first};
    bool const buffer_found{rel_pair.second};

    if (buffer_released || !buffer_found)
    {
        client_queue.push_back(released_buffer);
        client_available_cv.notify_one();
    }
}

void mc::BufferSwapperMulti::force_client_abort()
{
    std::unique_lock<std::mutex> lk(swapper_mutex);
    force_clients_to_complete = true;
    client_available_cv.notify_all();
}

void mc::BufferSwapperMulti::force_requests_to_complete()
{
    if (in_use_by_client == swapper_size - 1 && clients_trying_to_acquire > 0)
    {
        BOOST_THROW_EXCEPTION(
            std::logic_error("BufferSwapperMulti is not able to force requests to complete:"
                             " the client is trying to acquire all buffers"));
    }

    if (client_queue.empty())
    {
        if (compositor_queue.empty())
        {
            BOOST_THROW_EXCEPTION(
                std::logic_error("BufferSwapperMulti is not able to force requests to complete:"
                                 " all buffers are acquired"));
        }

        auto dequeued_buffer = compositor_queue.front();
        compositor_queue.pop_front();
        client_queue.push_back(dequeued_buffer);
    }

    client_available_cv.notify_all();
}


void mc::BufferSwapperMulti::end_responsibility(std::vector<std::shared_ptr<Buffer>>& buffers,
                                                size_t& size)
{
    std::unique_lock<std::mutex> lk(swapper_mutex);

    while(!compositor_queue.empty())
    {
        buffers.push_back(compositor_queue.back());
        compositor_queue.pop_back();
    }

    while(!client_queue.empty())
    {
        buffers.push_back(client_queue.back());
        client_queue.pop_back();
    }

    size = swapper_size;
}
