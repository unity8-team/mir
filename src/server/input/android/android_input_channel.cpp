/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "android_input_channel.h"

#include <androidfw/InputTransport.h>
#include <std/Errors.h>

#include <boost/exception/errinfo_errno.hpp>
#include <boost/throw_exception.hpp>

#include <cmath>

#include <unistd.h>

namespace mia = mir::input::android;
namespace droidinput = android;

namespace
{
void react_to_result(droidinput::status_t result)
{
    if (result!=droidinput::OK)
    {
        BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failure sending input event ")) << boost::errinfo_errno(result));
    }
}
}

mia::AndroidInputChannel::AndroidInputChannel()
{
    droidinput::InputChannel::openInputFdPair(s_fd, c_fd);
    channel = new droidinput::InputChannel(droidinput::String8("TODO: Name"), s_fd);
}

droidinput::sp<droidinput::InputChannel> mia::AndroidInputChannel::get_android_channel() const
{
    return channel;
}

mia::AndroidInputChannel::~AndroidInputChannel()
{
    close(s_fd);
    close(c_fd);
}

int mia::AndroidInputChannel::client_fd() const
{
    return c_fd;
}

int mia::AndroidInputChannel::server_fd() const
{
    return s_fd;
}

void mia::AndroidInputChannel::send_event(uint32_t seq, MirEvent const& event) const
{
    switch(event.type)
    {
    case mir_event_type_key:
        send_key_event(seq, event.key);
        break;
    case mir_event_type_motion:
        send_motion_event(seq, event.motion);
        break;
    default:
        break;
    }
}

void mia::AndroidInputChannel::send_motion_event(uint32_t seq, MirMotionEvent const& event) const
{
    droidinput::PointerCoords coords[MIR_INPUT_EVENT_MAX_POINTER_COUNT];
    droidinput::PointerProperties properties[MIR_INPUT_EVENT_MAX_POINTER_COUNT];
    for (size_t i = 0; i < event.pointer_count; i++)
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

    droidinput::InputPublisher publisher(channel);

    react_to_result(publisher.publishMotionEvent(seq,
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
                                                 coords));
}

void mia::AndroidInputChannel::send_key_event(uint32_t seq, MirKeyEvent const& event) const
{
    droidinput::InputPublisher publisher(channel);

    react_to_result(publisher.publishKeyEvent(seq,
                                              event.device_id,
                                              event.source_id,
                                              event.action,
                                              event.flags,
                                              event.key_code,
                                              event.scan_code,
                                              event.modifiers,
                                              event.repeat_count,
                                              event.down_time,
                                              event.event_time));
}

