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
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
  : input_channel(input_channel),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    looper(new droidinput::Looper(true)),
    fd_added(false),
    xkb_mapper(std::make_shared<mircv::XKBMapper>()),
    android_clock(clock),
    has_new_frame_time(false),
    frame_time(-1)
{
}

mircva::InputReceiver::InputReceiver(int fd,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
  : input_channel(new droidinput::InputChannel(droidinput::String8(""), fd)),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    looper(new droidinput::Looper(true)),
    fd_added(false),
    xkb_mapper(std::make_shared<mircv::XKBMapper>()),
    android_clock(clock),
    has_new_frame_time(false),
    frame_time(-1)
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

}

bool mircva::InputReceiver::try_next_event(MirEvent &ev)
{
    droidinput::InputEvent *android_event;
    uint32_t event_sequence_id;

    droidinput::status_t status;

    bool result = false;

    if ((status = input_consumer->consume(&event_factory, true, has_new_frame_time ? frame_time : -1,
        &event_sequence_id, &android_event))
            == droidinput::OK)
    {
        mia::Lexicon::translate(android_event, ev);
        map_key_event(xkb_mapper, ev);

        input_consumer->sendFinishedSignal(event_sequence_id, true);

        report->received_event(ev);

        result = true;
    }
    
    has_new_frame_time = false;

    return result;
}

// TODO: We use a droidinput::Looper here for polling functionality but it might be nice to integrate
// with the existing client io_service ~racarr ~tvoss
bool mircva::InputReceiver::next_event(std::chrono::milliseconds const& timeout, MirEvent &ev)
{
    if (!fd_added)
    {
        // TODO: Why will this fail from the constructor? ~racarr
        looper->addFd(fd(), fd(), ALOOPER_EVENT_INPUT, nullptr, nullptr);
        fd_added = true;
    }
    
    auto reduced_timeout = timeout;
    
    if (input_consumer->hasPendingBatch())
        reduced_timeout = std::chrono::milliseconds(1);

    int result = 0;
    if (!input_consumer->hasDeferredEvent())
        result = looper->pollOnce(reduced_timeout.count());

    std::lock_guard<std::mutex> lg(frame_time_guard);

    // If we are awoken and not by a frame time it is for shutdown.
    if (result == ALOOPER_POLL_WAKE && has_new_frame_time == false)
        return false;

    if (result == ALOOPER_POLL_ERROR) // TODO: Exception?
       return false;

    return try_next_event(ev);
}

void mircva::InputReceiver::wake()
{
    looper->wake();
}

void mircva::InputReceiver::notify_of_frame_time(std::chrono::nanoseconds new_frame_time)
{
    {
        std::lock_guard<std::mutex> lg(frame_time_guard);
        has_new_frame_time = true;
        if (new_frame_time != std::chrono::nanoseconds::min())
            frame_time = new_frame_time.count();
        else
            frame_time = -1;
    }
    wake();
}
