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

#ifndef MIR_COMPOSITOR_BUFFER_SWAPPER_H_
#define MIR_COMPOSITOR_BUFFER_SWAPPER_H_

#include <vector>
#include <memory>

namespace mir
{
namespace compositor
{
class Buffer;

class BufferSwapper
{
public:
    /* callers of client_acquire are returned a pointer to the
      currently usable buffer. Depending on the swapper, this call may potentially wait
      for a buffer to become available */
    virtual std::shared_ptr<Buffer> client_acquire() = 0;

    /* once a client is done with the finished buffer, it must queue
       it. This modifies the buffer the compositor posts to the screen */
    virtual void client_release(std::shared_ptr<Buffer> const& queued_buffer) = 0;

    /**
     * Acquires the last posted buffer.
     *
     * Callers of compositor_acquire() should get no-wait access to the
     * last posted buffer. However, the client will potentially stall
     * until control of the buffer is returned via matching calls to
     * compositor_release().
     *
     * Each call in a series of compositor_acquire() calls is guaranteed to
     * return a buffer no older than the ones returned in the previous calls
     * (may return the same buffer).
     */
    virtual std::shared_ptr<Buffer> compositor_acquire() = 0;

    /**
     * Releases a buffer from the compositor to the client.
     *
     * Note that the buffer is made available to the client only when all users
     * who have called compositor_acquire() have released the buffer.
     */
    virtual void compositor_release(std::shared_ptr<Buffer> const& released_buffer) = 0;

    /**
     * Forces client requests on the buffer swapper to abort.
     *
     * client_acquire is the only function that can block to provide sync.
     * This function unblocks client_acquire, generally resulting in an exception
     * in threads with a waiting client_acquire()
     *
     * After this request, the compositor can keep acquiring and releasing buffers
     * but the client cannot. This used in shutdown of the swapper, the client cannot
     * be reactivated after this call completes.
     */
    virtual void force_client_abort() = 0;

    /**
     * Forces requests on the buffer swapper to complete.
     *
     * Requests, like client_acquire(), can block waiting for a buffer to
     * become available. This method tries to force the request to
     * complete while ensuring that swapper keeps functioning properly.
     *
     * Note that it's not always possible to force a request to complete
     * without breaking the swapper. It's a logic error to attempt to call
     * this method if the circumstances don't allow a forced completion.
     *
     * To successfully use this method you should ensure that (i.e. the
     * preconditions for calling this method are):
     *
     * - The compositor is not holding any buffers (e.g., it has been stopped).
     * - The client is trying to acquire at most one buffer at a time (which is
     *   normal usage, and enforced by the high level API).
     */
    virtual void force_requests_to_complete() = 0;

    /**
     * Ends synchronization of buffers. All buffers owned by the swapper
     * at the time of this call are transferred to the 'buffers' parameter.
     * The swapper specifies the number of buffers it was managing (buffers owned by
     * the swapper plus any outstanding buffers) in 'original_size'. 
     * After the completion of this call, neither the client, nor the compositor can
     * continue to request or return buffers to this swapper.
     */
    virtual void end_responsibility(std::vector<std::shared_ptr<Buffer>>& buffers, size_t& original_size) = 0;

    /**
     * If the swapper has been used, and you want to preserve the buffers that have been used, 
     * it is advisable to shutdown the BufferSwapper  by using force_client_abort()
     * and then end_responsibility(). If these are not called, all buffers within the swapper
     * will be deallocated
     */
    virtual ~BufferSwapper() = default;
protected:
    BufferSwapper() = default;
    BufferSwapper(BufferSwapper const&) = delete;
    BufferSwapper& operator=(BufferSwapper const&) = delete;
};

}
}

#endif /* MIR_COMPOSITOR_BUFFER_SWAPPER_H_ */
