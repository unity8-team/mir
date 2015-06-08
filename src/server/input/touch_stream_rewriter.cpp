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

#include "mir_toolkit/event.h"

#include "mir/events/event_private.h"

#include <string.h>

#include <unordered_set>

namespace mi = mir::input;
namespace mev = mir::events;

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
mir::EventUPtr make_event_uptr(MirEvent *e)
{
    return mir::EventUPtr(e, delete_event);
}

mir::EventUPtr copy_event(MirTouchEvent const* ev)
{
    MirEvent *ret = new MirEvent;
    memcpy(ret, ev, sizeof(MirEvent));
    return make_event_uptr(ret);
}

mir::EventUPtr convert_touch_actions_to_change(MirTouchEvent const* ev)
{
    auto ret = copy_event(ev);

    for (size_t i = 0; i < ret->motion.pointer_count; i++)
    {
        ret->motion.pointer_coordinates[i].action = mir_touch_action_change;
    }
    return ret;
}

size_t index_for_id(MirTouchEvent const* touch_ev, MirTouchId id)
{
    MirEvent const* ev = reinterpret_cast<MirEvent const*>(touch_ev);
    for (size_t i = 0; i < ev->motion.pointer_count; i++)
    {
        if (ev->motion.pointer_coordinates[i].id == id)
            return i;
    }
    return 0;
}

mir::EventUPtr add_missing_down(MirTouchEvent const* valid_ev, MirTouchEvent const* ev, MirTouchId missing_id)
{
    // So as not to repeat already occurred actions, we copy the last valid (Delivered) event and replace all the actions
    // with change, then we will add a missing down for touch "missing_id"
    auto ret = convert_touch_actions_to_change(valid_ev);
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

mir::EventUPtr add_missing_up(MirTouchEvent const* valid_ev, MirTouchId missing_an_up_id)
{
    // Because we can only have one action per ev, we copy the last valid (Delivered) event and replace all the actions
    // with change, then we will change the action for the ID which should have gone up to an up.
    auto ret = convert_touch_actions_to_change(valid_ev);
    auto index = index_for_id(valid_ev, missing_an_up_id);

    ret->motion.pointer_coordinates[index].action = mir_touch_action_up;

    return ret;
}    
}

typedef std::unordered_set<MirTouchId> TouchSet;

void mi::TouchStreamRewriter::ensure_stream_validity_locked(std::lock_guard<std::mutex> const& lg, MirTouchEvent const* ev, MirTouchEvent const* last_ev)
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
        if (expected.find(id) == expected.end() && mir_touch_event_action(ev, i) != mir_touch_action_down)
        {
            auto inject_ev = add_missing_down(last_ev, ev, id);
            next_dispatcher->dispatch(*inject_ev);
            ensure_stream_validity_locked(lg, ev, reinterpret_cast<MirTouchEvent*>(inject_ev.get()));
            return;
        }
    }

    // Insert missing touch releases
    for (auto const& expected_id : expected)
    {
        if (found.find(expected_id) == found.end())
        {
            auto inject_ev = add_missing_up(last_ev, expected_id);
            next_dispatcher->dispatch(*inject_ev);
            ensure_stream_validity_locked(lg, ev, reinterpret_cast<MirTouchEvent*>(inject_ev.get()));
        }
    }
}


void mi::TouchStreamRewriter::handle_touch_event(MirInputDeviceId id, MirTouchEvent const* ev)
{
    std::lock_guard<std::mutex> lg(state_guard);

    auto it = last_event_by_device.find(id);
    MirTouchEvent const* last_ev = nullptr;
    auto default_ev = mev::make_event(id, std::chrono::high_resolution_clock::now().time_since_epoch(), mir_input_event_modifier_none); 

    if (it == last_event_by_device.end())
    {
        last_event_by_device.insert(std::make_pair(id, copy_event(ev)));
        last_ev = reinterpret_cast<MirTouchEvent const*>(default_ev.get());
    }
    else
    {
        last_ev = mir_input_event_get_touch_event(mir_event_get_input_event(it->second.get()));
    }
    
    ensure_stream_validity_locked(lg, ev, last_ev);

    // Seems to be no better way to replace a non default constructible value type in an unordered_map
    // C++17 will give us insert_or_assign
    last_event_by_device.erase(id);
    last_event_by_device.insert(std::make_pair(id, copy_event(ev)));
    
    next_dispatcher->dispatch(*reinterpret_cast<MirEvent const*>(ev));
}

void mi::TouchStreamRewriter::start()
{
}

void mi::TouchStreamRewriter::stop()
{
}
