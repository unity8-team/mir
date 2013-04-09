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
    virtual void focus_session(std::string client_name) = 0;
};

class NullDMMessageHandler : public DMMessageHandler
{
public:
    void focus_session(std::string client_name) {};
};

class DMConnection
{
public:
    DMConnection(int from_dm_fd, int to_dm_fd) :
        handler(std::make_shared<NullDMMessageHandler>()),
        from_dm_pipe(io_service, from_dm_fd),
        to_dm_pipe(io_service, to_dm_fd) {};

    void set_handler(std::shared_ptr<DMMessageHandler> const& handler)
    {
        this->handler = handler;
    }

    void start();

private:
    std::shared_ptr<DMMessageHandler> handler;
    boost::asio::io_service io_service;
    boost::asio::posix::stream_descriptor from_dm_pipe;
    boost::asio::posix::stream_descriptor to_dm_pipe;
    static size_t const size_of_header = 4;
    unsigned char message_header_bytes[size_of_header];
    boost::asio::streambuf payload;

    void read_header();
    void on_read_header(const boost::system::error_code& ec);
    void on_read_payload(const boost::system::error_code& ec);
};

#endif /* DM_CONNECTION_H_ */
