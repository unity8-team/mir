/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "android_input_receiver.h"

#include "mir/input/xkb_mapper.h"
#include "mir/input/input_receiver_report.h"
#include "mir/input/android/android_input_lexicon.h"

#include <androidfw/InputTransport.h>
#include <utils/Looper.h>

namespace mircv = mir::input::receiver;
namespace mircva = mircv::android;

namespace mia = mir::input::android;

mircva::InputReceiver::InputReceiver(droidinput::sp<droidinput::InputChannel> const& input_channel,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report)
  : input_channel(input_channel),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    looper(new droidinput::Looper(true)),
    fd_added(false),
    xkb_mapper(std::make_shared<mircv::XKBMapper>())
{
}

mircva::InputReceiver::InputReceiver(int fd,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report)
  : input_channel(new droidinput::InputChannel(droidinput::String8(""), fd)),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    looper(new droidinput::Looper(true)),
    fd_added(false),
    xkb_mapper(std::make_shared<mircv::XKBMapper>())
{
}

mircva::InputReceiver::~InputReceiver()
{
}

int mircva::InputReceiver::fd() const
{
    return input_channel->getFd();
}

namespace
{

static void map_key_event(std::shared_ptr<mircv::XKBMapper> const& xkb_mapper, MirEvent &ev)
{
    // TODO: As XKBMapper is used to track modifier state we need to use a seperate instance
    // of XKBMapper per device id (or modify XKBMapper semantics)
    if (ev.type != mir_event_type_key)
        return;

    xkb_mapper->update_state_and_map_event(ev.key);
}

bool compatible(MirEvent const& a, MirEvent const& b)
{
    if (a.type == mir_event_type_motion && b.type == mir_event_type_motion &&
        a.motion.device_id == b.motion.device_id &&
        a.motion.source_id == b.motion.source_id &&
        a.motion.action == b.motion.action &&
        a.motion.flags == b.motion.flags &&
        a.motion.modifiers == b.motion.modifiers &&
        a.motion.edge_flags == b.motion.edge_flags &&
        a.motion.button_state == b.motion.button_state &&
        //a.motion.x_offset == b.motion.x_offset &&
        //a.motion.y_offset == b.motion.y_offset &&
        //a.motion.x_precision == b.motion.x_precision &&
        a.motion.down_time == b.motion.down_time &&
        a.motion.pointer_count == b.motion.pointer_count)
    {
        return true;
    }
    return false;
}

}

int mircva::InputReceiver::try_next_event(MirEvent &out)
{
    droidinput::InputEvent *android_event;
    uint32_t event_sequence_id;

    // Input events use CLOCK_REALTIME, and so we must...
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    nsecs_t now = ts.tv_sec * 1000000000LL + ts.tv_nsec;

    while (input_consumer->consume(&event_factory, true,
           -1, &event_sequence_id, &android_event) != droidinput::WOULD_BLOCK
           && buffer.size() < 1000)
    {

        buffer.resize(buffer.size() + 1);
        auto& in = buffer.back();

        mia::Lexicon::translate(android_event, in);
        map_key_event(xkb_mapper, in);
        input_consumer->sendFinishedSignal(event_sequence_id, true);
        report->received_event(in);
    }

    fprintf(stderr, "Buffer %d\n", (int)buffer.size());

    (void)now;
    static nsecs_t last_purge = 0;
    if (!buffer.empty() &&
        (!last_purge || (now - last_purge) > 16000000LL))
    {
        last_purge = now;
        while (buffer.size() > 1 && compatible(buffer[0], buffer[1]))
        {
            fprintf(stderr, "DROP\n");
            buffer.pop_front();
        }
        out = buffer.front();
        buffer.pop_front();
        return 1;
    }

    return 0;
}

// TODO: We use a droidinput::Looper here for polling functionality but it might be nice to integrate
// with the existing client io_service ~racarr ~tvoss
int mircva::InputReceiver::next_event(std::chrono::milliseconds const& timeout, MirEvent &ev)
{
    if (!fd_added)
    {
        // TODO: Why will this fail from the constructor? ~racarr
        looper->addFd(fd(), fd(), ALOOPER_EVENT_INPUT, nullptr, nullptr);
        fd_added = true;
    }

    int ret = try_next_event(ev);
    if (ret)
        return ret;

    if (buffer.empty())
    {
        auto result = looper->pollOnce(timeout.count());
        if (result == ALOOPER_POLL_WAKE)
            return 0;
        if (result == ALOOPER_POLL_ERROR) // TODO: Exception?
            return -1;
    }

    return try_next_event(ev);
}

void mircva::InputReceiver::wake()
{
    looper->wake();
}
