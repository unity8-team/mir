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

#ifndef MIR_TEST_DOUBLES_NULL_EVENT_SINK_H_
#define MIR_TEST_DOUBLES_NULL_EVENT_SINK_H_

#include "mir/frontend/event_sink.h"

#include <memory>

namespace mir
{
namespace frontend
{
class MessageSender;
}

namespace test
{
namespace doubles
{
struct NullEventSink : public frontend::EventSink
{
    void handle_event(MirEvent const&) {}
    void handle_lifecycle_event(MirLifecycleState) {}
    void handle_display_config_change(graphics::DisplayConfiguration const&) {}
    void send_ping(int32_t) {}
    void send_buffer(frontend::BufferStreamId, graphics::Buffer&, graphics::BufferIpcMsgType) {}
};

std::unique_ptr<frontend::EventSink> null_sink_factory(std::shared_ptr<frontend::MessageSender> const&);
}
}
}

#endif /* MIR_TEST_DOUBLES_NULL_EVENT_SINK_H_*/
