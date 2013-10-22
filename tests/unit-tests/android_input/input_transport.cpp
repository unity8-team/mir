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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include <androidfw/InputTransport.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace android;

namespace
{
struct AndroidInputTransport : public ::testing::Test
{
    void SetUp()
    {
    }

    void test_trasmission_through_channel(InputMessage *message)
    {
        int server_fd, client_fd;
        status_t status;

        status = InputChannel::openInputFdPair(server_fd, client_fd);
        ASSERT_EQ(OK, status);

        sp<InputChannel> serverChannel = new InputChannel("server", server_fd);
        sp<InputChannel> clientChannel = new InputChannel("client", client_fd);

        status = clientChannel->sendMessage(message);
        ASSERT_EQ(OK, status);

        InputMessage messageReceived;
        serverChannel->receiveMessage(&messageReceived);

        ASSERT_EQ(message->header.type, messageReceived.header.type);
        ASSERT_EQ(message->body.acceptedRejected.pointerId, messageReceived.body.acceptedRejected.pointerId);

        serverChannel.clear();
        clientChannel.clear();
        close(server_fd);
        close(client_fd);
    }
};
}

/*
  Checks that an "Accepted" type of message is correctly transmitted through
  an input channel.
 */
TEST_F(AndroidInputTransport, send_and_receive_accepted_message)
{
    InputMessage message;
    message.header.type = InputMessage::TYPE_ACCEPTED;
    message.body.acceptedRejected.pointerId = 123;

    test_trasmission_through_channel(&message);
}

/*
  Checks that a "Rejected" type of message is correctly transmitted through
  an input channel.
 */
TEST_F(AndroidInputTransport, send_and_receive_rejected_message)
{
    InputMessage message;
    message.header.type = InputMessage::TYPE_REJECTED;
    message.body.acceptedRejected.pointerId = 123;

    test_trasmission_through_channel(&message);
}
