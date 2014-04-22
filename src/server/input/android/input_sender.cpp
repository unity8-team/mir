/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "input_sender.h"

#include "mir/input/input_send_entry.h"
#include "mir/input/input_send_observer.h"
#include "mir/input/input_channel.h"
#include "mir/input/surface.h"
#include "mir/main_loop.h"

#include "androidfw/Input.h"
#include "androidfw/InputTransport.h"
#include "std/Errors.h"
#include "std/String8.h"

#include <boost/exception/errinfo_errno.hpp>
#include <boost/throw_exception.hpp>

#include <cstring>

namespace mi = mir::input;
namespace mia = mi::android;

namespace droidinput = android;

namespace
{
void check(droidinput::status_t result)
{
    if (result!=droidinput::OK)
    {
        BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failure sending input event ")) << boost::errinfo_errno(result));
    }
}
}

mia::InputSender::InputSender(std::shared_ptr<mir::graphics::EventHandlerRegister> const& registrar) :
    seq(), loop(registrar)
{
}

void mia::InputSender::set_send_observer(std::shared_ptr<InputSendObserver> const& observer)
{
    this->observer = observer;
}

mia::InputSender::ActiveTransfer & mia::InputSender::get_active_transfer(int server_fd)
{
    auto pos = transfers.find( server_fd );

    if (pos!=transfers.end())
        return pos->second;

    ActiveTransfer& transfer =
        transfers.emplace(std::piecewise_construct, std::forward_as_tuple(server_fd), std::forward_as_tuple(server_fd))
            .first->second;
    loop->register_fd_handler({server_fd},
                              [this,&transfer](int)
                              {
                                  this->handle_finish_signal(transfer);
                              }
                             );
    return transfer;
}

std::shared_ptr<mi::InputSendEntry> mia::InputSender::send_event(MirEvent const& event, Surface const& , std::shared_ptr<InputChannel> const& channel)
{
    ActiveTransfer *transfer = nullptr;
    std::shared_ptr<mi::InputSendEntry> result;
    {
        std::unique_lock<std::mutex> lock(sender_mutex);
        result = std::make_shared<mi::InputSendEntry>(next_seq(), event);

        transfer = &get_active_transfer(channel->server_fd());
    }

    transfer->send(result);
    return result;
}

uint32_t mia::InputSender::next_seq()
{
    while(!++seq);
    return seq;
}

mia::InputSender::ActiveTransfer::ActiveTransfer(int server_fd)
    : publisher(droidinput::sp<droidinput::InputChannel>(
          new droidinput::InputChannel(droidinput::String8(), server_fd)))
{
}

void mia::InputSender::ActiveTransfer::send(std::shared_ptr<InputSendEntry> const& event)
{
    {
        std::unique_lock<std::mutex> lock(transfer_mutex);
        switch(event->event.type)
        {
        case mir_event_type_key:
        case mir_event_type_motion:
            pending_responses.push_back(event);
            break;
        default:
            break;
        }
    }
    switch(event->event.type)
    {
    case mir_event_type_key:
        send_key_event(event->sequence_id, event->event.key);
        break;
    case mir_event_type_motion:
        send_motion_event(event->sequence_id, event->event.motion);
        break;
    default:
        break;
    }
}

void mia::InputSender::ActiveTransfer::send_key_event(uint32_t seq, MirKeyEvent const& event)
{
    check(
        publisher.publishKeyEvent(
            seq,
            event.device_id,
            event.source_id,
            event.action,
            event.flags,
            event.key_code,
            event.scan_code,
            event.modifiers,
            event.repeat_count,
            event.down_time,
            event.event_time
            )
        );
}

void mia::InputSender::ActiveTransfer::send_motion_event(uint32_t seq, MirMotionEvent const& event)
{
    droidinput::PointerCoords coords[MIR_INPUT_EVENT_MAX_POINTER_COUNT];
    droidinput::PointerProperties properties[MIR_INPUT_EVENT_MAX_POINTER_COUNT];
    // no default constructor:
    std::memset(coords, 0, sizeof(coords));
    std::memset(properties, 0, sizeof(properties));

    for (size_t i = 0; i < event.pointer_count; ++i)
    {
        // TODO this assumes that: x == raw_x + x_offset;
        // here x,y is used instead of the raw co-ordinates and offset is set to zero
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_X, event.pointer_coordinates[i].x);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, event.pointer_coordinates[i].y);

        coords[i].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR, event.pointer_coordinates[i].touch_major);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR, event.pointer_coordinates[i].touch_minor);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_SIZE, event.pointer_coordinates[i].size);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_PRESSURE, event.pointer_coordinates[i].pressure);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_ORIENTATION, event.pointer_coordinates[i].orientation);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_VSCROLL, event.pointer_coordinates[i].vscroll);
        coords[i].setAxisValue(AMOTION_EVENT_AXIS_HSCROLL, event.pointer_coordinates[i].hscroll);
        properties[i].toolType = event.pointer_coordinates[i].tool_type;
    }

    check(
        publisher.publishMotionEvent(
            seq,
            event.device_id,
            event.source_id,
            event.action,
            static_cast<int32_t>(event.flags),
            event.edge_flags,
            static_cast<int32_t>(event.modifiers),
            static_cast<int32_t>(event.button_state),
            0.0f,  // event.x_offset,
            0.0f,  // event.y_offset,
            event.x_precision,
            event.y_precision,
            event.down_time,
            event.event_time,
            event.pointer_count,
            properties,
            coords
            )
        );

}

void mia::InputSender::ActiveTransfer::submit_result(std::shared_ptr<InputSendObserver> const& observer)
{
    uint32_t sequence;
    bool handled;

    while(true)
    {
        droidinput::status_t status = publisher.receiveFinishedSignal(&sequence, &handled);

        if (status==droidinput::OK)
        {
            std::shared_ptr<InputSendEntry> entry = unqueue_entry(sequence);

            if (entry && observer)
                observer->send_suceeded(entry, handled ? InputSendObserver::Consumed : InputSendObserver::NotConsumed);
        }
        else return;
        // TODO handle all the other cases..
    }

}

std::shared_ptr<mi::InputSendEntry> mia::InputSender::ActiveTransfer::unqueue_entry(uint32_t sequence_id)
{
    std::unique_lock<std::mutex> lock(transfer_mutex);
    auto pos = std::find_if(pending_responses.begin(),
                            pending_responses.end(),
                            [sequence_id](std::shared_ptr<mi::InputSendEntry> const& entry)
                            { return entry->sequence_id == sequence_id; });
    std::shared_ptr<mi::InputSendEntry> result = *pos;
    pending_responses.erase(pos);
    return result;
}

void mia::InputSender::handle_finish_signal(ActiveTransfer & transfer)
{
    transfer.submit_result(observer);
}

