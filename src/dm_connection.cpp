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
#include "dm_protocol.pb.h"

#include <boost/signals2.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

void DMConnection::start()
{
    read_header();
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
                       payload,
                       ba::transfer_exactly(payload_length),
                       boost::bind(&DMConnection::on_read_payload,
                                   this,
                                   ba::placeholders::error));
    }
}

void DMConnection::on_read_payload(const bs::error_code& ec)
{
    if (!ec)
    {
        size_t const message_id = message_header_bytes[0] << 8 | message_header_bytes[1];
        std::istream p(&payload);
      
        switch (message_id)
        {
        case 0:
        {
            FocusSession message;
            message.ParseFromIstream(&p);
            std::cerr << message.client_name() << std::endl;
        }          
        default:
            break;
        }
    }

    read_header();
}
