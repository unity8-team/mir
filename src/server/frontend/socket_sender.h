/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_FRONTEND_SOCKET_SENDER_H_ 
#define MIR_FRONTEND_SOCKET_SENDER_H_ 
#include "message_sender.h"
#include "message_receiver.h"

namespace mir
{
namespace frontend
{
namespace detail
{
class SocketSender : public MessageSender, public MessageReceiver
{
public:
    SocketSender(boost::asio::local::stream_protocol::socket && socket);

    /* MessageSender */
    void send(std::string const& body);
    void send_fds(std::vector<int32_t> const& fds);

    /* MessageReceiver */
    void async_receive_msg(std::function<void(boost::system::error_code const&)> const& handler, boost::asio::streambuf buffer, size_t size); 
    pid_t client_pid();
    
private:
    boost::asio::local::stream_protocol::socket socket;
    std::vector<char> whole_message;
};
}
}
}

#endif /* MIR_FRONTEND_SOCKET_SENDER_H_ */ 
