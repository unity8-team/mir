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
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 */

#ifndef DM_CONNECTION_H_
#define DM_CONNECTION_H_

#include <boost/asio.hpp>

class DMMessageHandler
{
public:
    virtual void set_active_session(std::string client_name) = 0;
};

class NullDMMessageHandler : public DMMessageHandler
{
public:
    void set_active_session(std::string client_name) {};
};

enum class USCMessageID
{
    ping = 0,
    pong = 1,
    ready = 2,
    session_connected = 3,
    set_active_session = 4
};

class DMConnection
{
public:
    DMConnection(boost::asio::io_service& io_service, int from_dm_fd, int to_dm_fd) :
        from_dm_pipe(io_service, from_dm_fd),
        to_dm_pipe(io_service, to_dm_fd) {};

    void set_handler(DMMessageHandler *handler)
    {
        this->handler = handler;
    }

    void start();

    void send_ready();

private:
    DMMessageHandler *handler;
    boost::asio::posix::stream_descriptor from_dm_pipe;
    boost::asio::posix::stream_descriptor to_dm_pipe;
    static size_t const size_of_header = 4;
    unsigned char message_header_bytes[size_of_header];
    boost::asio::streambuf message_payload_buffer;
    std::vector<char> write_buffer;

    void read_header();
    void on_read_header(const boost::system::error_code& ec);
    void on_read_payload(const boost::system::error_code& ec);
    void send(USCMessageID id, std::string const& body);
};

#endif /* DM_CONNECTION_H_ */
