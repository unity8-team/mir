/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */


#ifndef MIR_CLIENT_RPC_STREAM_TRANSPORT_H_
#define MIR_CLIENT_RPC_STREAM_TRANSPORT_H_

#include <vector>
#include <memory>
#include <stdint.h>

namespace mir
{
namespace client
{
namespace rpc
{
/**
 * \brief Responsible for shuttling bytes to and from the server
 *
 * This is a transport providing stream semantics. It does not preserve message
 * boundaries, writes from the remote end are not guaranteed to become available
 * for reading atomically, and local writes are not guaranteed to become available
 * for reading at the remote end atomically.
 *
 * In practice reads and writes of “small size” will be atomic. See your kernel
 * source tree for a definition of “small size” :).
 *
 * As it does not preserve message boundaries, partial reads do not discard any
 * unread data waiting for reading. This applies for both binary data and file descriptors.
 *
 * \note It is safe to call a read and a write method simultaneously
 *       from different threads. Multiple threads calling the same
 *       function need synchronisation.
 */
class StreamTransport
{
public:
    /**
     * \note Upon completion of the destructor it is guaranteed that no methods will be called
     *       by the StreamTransport on any Observer that had been registered.
     */
    virtual ~StreamTransport() = default;

    /**
     * \brief Observer of IO status
     * \note The Transport may call Observer members from arbitrary threads.
     *       The Observer implementation is responsible for any synchronisation.
     */
    class Observer
    {
    public:
        virtual ~Observer() = default;
        /**
         * \brief Called by the Transport when data is available for reading
         */
        virtual void on_data_available() = 0;
        /**
         * \brief Called by the Transport when the connection to the server has been broken.
         * \note This is not guaranteed to be triggered exactly once; it may not fire
         *       during destruction of the Transport, or it may fire multiple times.
         */
        virtual void on_disconnected() = 0;
    };

    /**
     * \brief Register an IO observer
     * \param [in] observer
     * \note There is no guarantee which thread will call into the observer.
     *       Synchronisation is the responsibility of the caller.
     */
    virtual void register_observer(std::shared_ptr<Observer> const& observer) = 0;

    /**
     * \brief Read data from the server
     * \param [out] buffer          Buffer to read into
     * \param [in]  read_bytes      Number of bytes to read
     * \throws A std::runtime_error if it is not possible to read
     *         read_bytes bytes from the server.
     *
     * \note This provides stream semantics - message boundaries are not preserved.
     */
    virtual void receive_data(void* buffer, size_t read_bytes) = 0;

    /**
     * \brief Read data and file descriptors from the server
     * \param [out] buffer          Buffer to read into
     * \param [in]  read_bytes      Number of bytes to read
     * \param [in,out] fds          File descriptors received in this read.
     *                              The value of fds.size() determines the number of
     *                              file descriptors to receive.
     * \throws A std::runtime_error if it is not possible to read
     *         read_bytes bytes from the server or if it is not possible to read
     *         fds.size() file descriptors from the server.
     *
     * \note This provides stream semantics - message boundaries are not preserved.
     */
    virtual void receive_data(void* buffer, size_t read_bytes, std::vector<int>& fds) = 0;

    /**
     * \brief Write data to the server
     * \param [in] buffer   Data to send
     * \throws A std::runtime_error if it is not possible to write the full contents
     *         of buffer to the server.
     */
    virtual void send_data(std::vector<uint8_t> const& buffer) = 0;

    /**
     * \brief Get a poll()able file descriptor
     * \return A file descriptor usable with poll() or equivalent function calls that
     *         becomes readable when there are dispatchable events
     */
    virtual int watch_fd() const = 0;
};

}
}
}

#endif // MIR_CLIENT_RPC_STREAM_TRANSPORT_H_
