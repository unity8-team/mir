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

#include "mir/dispatch/multiplexing_dispatchable.h"
#include "mir/input/xkb_mapper.h"
#include "mir/input/input_receiver_report.h"
#include "mir/input/android/android_input_lexicon.h"
#include "mir/events/event_private.h"

#include <boost/throw_exception.hpp>

#include <androidfw/InputTransport.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <system_error>
#include <cstdlib>

namespace mircv = mir::input::receiver;
namespace mircva = mircv::android;
namespace md = mir::dispatch;

namespace mia = mir::input::android;

mircva::InputReceiver::InputReceiver(droidinput::sp<droidinput::InputChannel> const& input_channel,
                                     std::shared_ptr<mircv::XKBMapper> const& keymapper,
                                     std::function<void(MirEvent*)> const& event_handling_callback,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
  : input_channel(input_channel),
    handler{event_handling_callback},
    xkb_mapper(keymapper),
    report(report),
    input_consumer(std::make_shared<droidinput::InputConsumer>(input_channel)),
    android_clock(clock)
{
    int pipefds[2];
    if (pipe(pipefds) < 0)
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to create notification pipe for IO"}));
    }
    notify_receiver_fd = mir::Fd{pipefds[0]};
    notify_sender_fd = mir::Fd{pipefds[1]};

    dispatcher.add_watch(notify_receiver_fd, [this]()
    {
        consume_wake_notification(notify_receiver_fd);
        process_and_maybe_send_event();
    });

    dispatcher.add_watch(mir::Fd{mir::IntOwnedFd{input_channel->getFd()}},
                         [this]() { process_and_maybe_send_event(); });
}

mircva::InputReceiver::InputReceiver(int fd,
                                     std::shared_ptr<mircv::XKBMapper> const& keymapper,
                                     std::function<void(MirEvent*)> const& event_handling_callback,
                                     std::shared_ptr<mircv::InputReceiverReport> const& report,
                                     AndroidClock clock)
    : InputReceiver(new droidinput::InputChannel(droidinput::String8(""), fd),
                    keymapper,
                    event_handling_callback,
                    report,
                    clock)
{
}

mircva::InputReceiver::~InputReceiver()
{
}

mir::Fd mircva::InputReceiver::watch_fd() const
{
    return dispatcher.watch_fd();
}

bool mircva::InputReceiver::dispatch(md::FdEvents events)
{
    return dispatcher.dispatch(events);
}

md::FdEvents mircva::InputReceiver::relevant_events() const
{
    return dispatcher.relevant_events();
}

namespace
{

static void map_key_event(std::shared_ptr<mircv::XKBMapper> const& xkb_mapper, MirEvent &ev)
{
    // TODO: As XKBMapper is used to track modifier state we need to use a seperate instance
    // of XKBMapper per device id (or modify XKBMapper semantics)
    if (mir_event_get_type(&ev) != mir_event_type_input)
        return;
    if (mir_input_event_get_type(mir_event_get_input_event(&ev)) !=
        mir_input_event_type_key)
        return;

    xkb_mapper->update_state_and_map_event(ev);
}

}

void mircva::InputReceiver::process_and_maybe_send_event()
{
    MirEvent ev;
    droidinput::InputEvent *android_event;
    uint32_t event_sequence_id;

    auto frame_time = std::chrono::nanoseconds(-1);

    auto result = input_consumer->consume(&event_factory,
                                          true,
                                          frame_time,
                                          &event_sequence_id,
                                          &android_event);
    if (result == droidinput::OK)
    {
        mia::Lexicon::translate(android_event, ev);

        map_key_event(xkb_mapper, ev);

        input_consumer->sendFinishedSignal(event_sequence_id, true);

        report->received_event(ev);

        // Send the event on its merry way.
        handler(&ev);
    }
    if (input_consumer->hasDeferredEvent())
    {
        // input_consumer->consume() can read an event from the fd and find that the event cannot
        // be added to the current batch.
        //
        // In this case, it emits the current batch and leaves the new event pending.
        // This means we have an event we need to dispatch, but as it has already been read from
        // the fd we cannot rely on being woken by the fd being readable.
        //
        // So, we ensure we'll appear dispatchable by pushing an event to the wakeup pipe.
        wake();
    }
    else if (input_consumer->hasPendingBatch())
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Logic error in InputConsumer"}));
    }
}

void mircva::InputReceiver::consume_wake_notification(mir::Fd const& fd)
{
    uint64_t dummy;
    if (read(fd, &dummy, sizeof(dummy)) != sizeof(dummy))
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to consume notification"}));
    }
}

void mircva::InputReceiver::wake()
{
    uint64_t dummy{0};
    if (write(notify_sender_fd, &dummy, sizeof(dummy)) != sizeof(dummy))
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to notify IO loop"}));
    }
}
