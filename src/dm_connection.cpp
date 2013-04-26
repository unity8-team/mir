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

#include "dm_connection.h"

#include <boost/signals2.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

void DMConnection::start()
{
    std::cerr << "dm_connection_start" << std::endl;
    read_header();
}

void DMConnection::send_ready()
{
    send(USCMessageID::ready, "");
}

void DMConnection::read_header()
{
    ba::async_read(from_dm_pipe,
                   ba::buffer(message_header_bytes),
                   boost::bind(&DMConnection::on_read_header,
                               this,
                               ba::placeholders::error));
}

void DMConnection::on_read_header(const bs::error_code& ec)
{
    if (!ec)
    {
        size_t const payload_length = message_header_bytes[2] << 8 | message_header_bytes[3];
        ba::async_read(from_dm_pipe,
                       message_payload_buffer,
                       ba::transfer_exactly(payload_length),
                       boost::bind(&DMConnection::on_read_payload,
                                   this,
                                   ba::placeholders::error));
    }
    else
        std::cerr << "Failed to read header" << std::endl;
}

void DMConnection::on_read_payload(const bs::error_code& ec)
{
    if (!ec)
    {
        auto message_id = (USCMessageID) (message_header_bytes[0] << 8 | message_header_bytes[1]);
        size_t const payload_length = message_header_bytes[2] << 8 | message_header_bytes[3];

        switch (message_id)
        {
        case USCMessageID::ping:
        {
            std::cerr << "ping" << std::endl;
            send(USCMessageID::pong, "");
            break;
        }
        case USCMessageID::pong:
        {
            std::cerr << "pong" << std::endl;
            break;
        }
        case USCMessageID::set_active_session:
        {
            std::ostringstream ss;
            ss << &message_payload_buffer;
            auto client_name = ss.str();
            std::cerr << "set_active_session '" << client_name << "'" << std::endl;
            if (handler)
                handler->set_active_session(client_name);
            break;
        }
        default:
            std::cerr << "Ignoring unknown message " << (uint16_t) message_id << " with " << payload_length << " octets" << std::endl;
            break;
        }
    }
    else
        std::cerr << "Failed to read payload" << std::endl;

    read_header();
}

void DMConnection::send(USCMessageID id, std::string const& body)
{
    const size_t size = body.size();
    const uint16_t _id = (uint16_t) id;
    const unsigned char header_bytes[4] =
    {
        static_cast<unsigned char>((_id >> 8) & 0xFF),
        static_cast<unsigned char>((_id >> 0) & 0xFF),
        static_cast<unsigned char>((size >> 8) & 0xFF),
        static_cast<unsigned char>((size >> 0) & 0xFF)
    };

    write_buffer.resize(sizeof header_bytes + size);
    std::copy(header_bytes, header_bytes + sizeof header_bytes, write_buffer.begin());
    std::copy(body.begin(), body.end(), write_buffer.begin() + sizeof header_bytes);

    // FIXME: Make asynchronous
    ba::write(to_dm_pipe, ba::buffer(write_buffer));
}
