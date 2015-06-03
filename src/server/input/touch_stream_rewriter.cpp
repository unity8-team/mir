/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "touch_stream_rewriter.h"

namespace mi = mir::input;

mi::TouchStreamRewriter::TouchStreamRewriter(std::shared_ptr<mi::InputDispatcher> const& next_dispatcher)
    : next_dispatcher(next_dispatcher)
{
}

void mi::TouchStreamRewriter::dispatch(MirEvent const& event)
{
    if (mir_event_get_type(&event) != mir_event_type_input)
    {
        next_dispatcher->dispatch(event);
        return;
    }
    auto iev = mir_event_get_input_event(&event);
    if (mir_input_event_get_type(iev) != mir_input_event_type_touch)
    {
        next_dispatcher->dispatch(event);
        return;
    }
    auto tev = mir_input_event_get_touch_event(iev);
    handle_touch_event(mir_input_event_get_device_id(iev), tev);
}

// TODO: Dedupe
namespace
{
void delete_event(MirEvent *e) { mir_event_unref(e); }
mev::EventUPtr make_event_uptr(MirEvent *e)
{
    return mir::EventUPtr(e, delete_event);
}

mev::EventUPtr add_missing_down(MirTouchInputEvent const* valid_ev, MirTouchInputEvent const* ev, MirTouchId missing_id)
{
    // Because we can only have one action per ev, we copy the last valid (Delivered) event and replace all the actions
    // with change, then we will add a missing down for touch "missing_id" 
    auto ret = convert_touch_actions_to_change(copy_ev(valid_ev));
    auto index = index_for_id(ev, missing_id);

    mev::add_touch(*ret, missing_id, mir_touch_action_down,
                   mir_touch_event_tooltype(ev, index),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_x),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_y),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_pressure),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_touch_major),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_touch_minor),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_size));
    return ret;
}

mev::EventUPtr add_missing_up(MirTouchInputEvent const* valid_ev, MirTouchInputEvent const* ev, MirTouchId missing_id)
{
    // Because we can only have one action per ev, we copy the last valid (Delivered) event and replace all the actions
    // with change, then we will add a missing down for touch "missing_id" 
    auto ret = convert_touch_actions_to_change(copy_ev(valid_ev));
    auto index = index_for_id(ev, missing_id);

    mev::add_touch(*ret, missing_id, mir_touch_action_up,
                   mir_touch_event_tooltype(ev, index),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_x),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_y),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_pressure),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_touch_major),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_touch_minor),
                   mir_touch_event_axis_value(ev, index, mir_touch_axis_size));
    return ret;
}    
}

typedef std::unordered_set<MirTouchId> TouchSet;

// Rework the case where there is no last_ev
void mi::TouchStreamRewriter::ensure_stream_validity_locked(std::lock_guard<std::mutex> const& lg, MirTouchInputEvent const* ev, MirTouchInputEvent const* last_ev)
{
    TouchSet expected;
    for (size_t i = 0; i < mir_touch_event_point_count(last_ev); i++)
    {
        auto action = mir_touch_event_action(last_ev, i);
        if (action == mir_touch_action_up)
            continue;
        expected.insert(mir_touch_event_id(last_ev, i));
    }

    TouchSet found;
    for (size_t i = 0; i < mir_touch_event_point_count(ev); i++)
    {
        auto id = mir_touch_event_id(ev, i);
        found.insert(id);
        if (expected.find(id) != expected.end())
        {
            // TODO: Inject event which is last event + down for this ID
            // REturns an event which is last_ev + a touch down from i
            auto inject_ev = add_missing_down(last_ev, ev, id);
            next_dispatcher->inject(inject_ev);
            ensure_stream_validity_locked(lg, ev, inject_ev);
            return;
        }
    }

    // Insert missing touch releases
    for (auto const& expected_id : expected)
    {
        auto inject_ev = add_missing_up(last_ev, ev, id);
        next_dispatcher->inject(inject_ev);
        ensure_stream_validity_locked(lg, ev, inject_ev);
    }
}


// What if the first event is invalid
void mi::TouchStreamRewriter::handle_touch_event(MirInputDeviceId id, MirTouchInputEvent const* ev)
{
    std::lock_guard<std::mutex> lg(state_guard);

    auto it = last_event_by_device.find(id);
    if (it == last_event_by_device.end())
    {
        last_event_by_device[id] = make_event_uptr(mir_event_ref(ev));
        return;
    }

    auto last_ev = mir_input_event_get_touch_input_event(mir_event_get_input_event(*it));

    ensure_stream_validity_locked(lg, ev, last_ev);
}

void mi::TouchStreamRewriter::start()
{
}

void mi::TouchStreamRewriter::stop()
{
}
