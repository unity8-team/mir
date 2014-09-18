/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "socket_messenger.h"
#include "mir/frontend/client_constants.h"
#include "mir/frontend/session_credentials.h"
#include "mir/variable_length_array.h"

#include <boost/throw_exception.hpp>

#include <errno.h>
#include <string.h>

#include <stdexcept>

namespace mf = mir::frontend;
namespace mfd = mf::detail;
namespace bs = boost::system;
namespace ba = boost::asio;

mfd::SocketMessenger::SocketMessenger(std::shared_ptr<ba::local::stream_protocol::socket> const& socket)
    : socket(socket)
{
    // Make the socket non-blocking to avoid hanging the server when a client
    // is unresponsive. Also increase the send buffer size to 64KiB to allow
    // more leeway for transient client freezes.
    // See https://bugs.launchpad.net/mir/+bug/1350207
    // TODO: Rework the messenger to support asynchronous sends
    socket->non_blocking(true);
    boost::asio::socket_base::send_buffer_size option(64*1024);
    socket->set_option(option);
}

mf::SessionCredentials mfd::SocketMessenger::creator_creds() const
{
    struct ucred cr;
    socklen_t cl = sizeof(cr);

    auto status = getsockopt(socket->native_handle(), SOL_SOCKET, SO_PEERCRED, &cr, &cl);

    if (status)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to query client socket credentials"));

    return {cr.pid, cr.uid, cr.gid};
}

mf::SessionCredentials mfd::SocketMessenger::client_creds()
{
    if (session_creds.pid() != 0)
        return session_creds;

    // We've not got the credentials from client yet.
    // Return the credentials that created the socket instead.
    return creator_creds();
}

void mfd::SocketMessenger::send(char const* data, size_t length, FdSets const& fd_set)
{
    static size_t const header_size{2};
    mir::VariableLengthArray<mf::serialization_buffer_size> whole_message{header_size + length};

    whole_message.data()[0] = static_cast<unsigned char>((length >> 8) & 0xff);
    whole_message.data()[1] = static_cast<unsigned char>((length >> 0) & 0xff);
    std::copy(data, data + length, whole_message.data() + header_size);

    std::unique_lock<std::mutex> lg(message_lock);

    // TODO: This should be asynchronous, but we are not making sure
    // that a potential call to send_fds is executed _after_ this
    // function has completed (if it would be executed asynchronously.
    // NOTE: we rely on this synchronous behavior as per the comment in
    // mf::SessionMediator::create_surface
    ba::write(*socket, ba::buffer(whole_message.data(), whole_message.size()));

    for (auto const& fds : fd_set)
        send_fds_locked(lg, fds);
}

void mfd::SocketMessenger::send_fds_locked(std::unique_lock<std::mutex> const&, std::vector<mir::Fd> const& fds)
{
    if (fds.size() > 0)
    {
        // We send dummy data
        struct iovec iov;
        char dummy_iov_data = 'M';
        iov.iov_base = &dummy_iov_data;
        iov.iov_len = 1;

        // Allocate space for control message
        static auto const builtin_n_fds = 5;
        static auto const builtin_cmsg_space = CMSG_SPACE(builtin_n_fds * sizeof(int));
        auto const fds_bytes = fds.size() * sizeof(int);
        mir::VariableLengthArray<builtin_cmsg_space> control{CMSG_SPACE(fds_bytes)};
        // Silence valgrind uninitialized memory complaint
        memset(control.data(), 0, control.size());

        // Message to send
        struct msghdr header;
        header.msg_name = NULL;
        header.msg_namelen = 0;
        header.msg_iov = &iov;
        header.msg_iovlen = 1;
        header.msg_controllen = control.size();
        header.msg_control = control.data();
        header.msg_flags = 0;

        // Control message contains file descriptors
        struct cmsghdr *message = CMSG_FIRSTHDR(&header);
        message->cmsg_len = CMSG_LEN(fds_bytes);
        message->cmsg_level = SOL_SOCKET;
        message->cmsg_type = SCM_RIGHTS;

        int* const data = reinterpret_cast<int*>(CMSG_DATA(message));
        int i = 0;
        for (auto& fd : fds)
            data[i++] = fd;

        auto const sent = sendmsg(socket->native_handle(), &header, 0);
        if (sent < 0)
            BOOST_THROW_EXCEPTION(std::runtime_error("Failed to send fds: " + std::string(strerror(errno))));
    }
}

void mfd::SocketMessenger::async_receive_msg(
    MirReadHandler const& handler,
    ba::mutable_buffers_1 const& buffer)
{
    boost::asio::async_read(
         *socket,
         buffer,
         boost::asio::transfer_exactly(ba::buffer_size(buffer)),
         handler);
}

bs::error_code mfd::SocketMessenger::receive_msg(
    ba::mutable_buffers_1 const& buffer)
{
    bs::error_code e;
    size_t nread = 0;

    while (nread < ba::buffer_size(buffer))
    {
        nread += boost::asio::read(
             *socket,
             ba::mutable_buffers_1{buffer + nread},
             e);

        if (e && e != ba::error::would_block)
            break;
    }

    return e;
}

size_t mfd::SocketMessenger::available_bytes()
{
    // We call available_bytes() once the client is talking to us
    // so this is a pragmatic place to grab the session credentials
    if (session_creds.pid() == 0)
        update_session_creds();

    boost::asio::socket_base::bytes_readable command{true};
    socket->io_control(command);
    return command.get();
}

void mfd::SocketMessenger::update_session_creds()
{
    union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(ucred))];
    } control_un;

    control_un.cmh.cmsg_len = CMSG_LEN(sizeof(ucred));
    control_un.cmh.cmsg_level = SOL_SOCKET;
    control_un.cmh.cmsg_type = SCM_CREDENTIALS;

    msghdr msgh;
    msgh.msg_name = nullptr;
    msgh.msg_namelen = 0;
    msgh.msg_iov = nullptr;
    msgh.msg_iovlen = 0;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    if (recvmsg(socket->native_handle(), &msgh, MSG_PEEK) != -1)
    {
        auto const ucredp = reinterpret_cast<ucred*>(CMSG_DATA(CMSG_FIRSTHDR(&msgh)));
        session_creds = {ucredp->pid, ucredp->uid, ucredp->gid};
    }
}
