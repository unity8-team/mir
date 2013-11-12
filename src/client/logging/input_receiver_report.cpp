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

#include "input_receiver_report.h"

#include "mir/logging/logger.h"

#include <boost/throw_exception.hpp>

#include <sstream>
#include <stdexcept>

namespace ml = mir::logging;
namespace mcll = mir::client::logging;

namespace
{
std::string const component{"input-receiver"};
}

mcll::InputReceiverReport::InputReceiverReport(std::shared_ptr<ml::Logger> const& logger)
    : logger{logger}
{
}

namespace
{

std::string format_key_action(MirKeyAction action)
{
    static std::string key_action_strings[] = { "mir_key_action_down", "mir_key_action_up",
                                                "mir_key_action_multiple" };
    return key_action_strings[action];
}

std::string format_key_flags(MirKeyFlag flags)
{
    std::stringstream ss;

    static struct { MirKeyFlag flag; char const* str; } const key_flag_strings[] = {
        { mir_key_flag_woke_here, "mir_key_flag_woke_here" },
        { mir_key_flag_soft_keyboard, "mir_key_flag_soft_keyboard" },
        { mir_key_flag_keep_touch_mode, "mir_key_flag_keep_touch_mode" },
        { mir_key_flag_from_system, "mir_key_flag_from_system" },
        { mir_key_flag_editor_action, "mir_key_flag_editor_action" },
        { mir_key_flag_canceled, "mir_key_flag_canceled" },
        { mir_key_flag_virtual_hard_key, "mir_key_flag_virtual_hard_key" },
        { mir_key_flag_long_press, "mir_key_flag_long_press" },
        { mir_key_flag_canceled_long_press, "mir_key_flag_canceled_long_press" },
        { mir_key_flag_tracking, "mir_key_flag_tracking" },
        { mir_key_flag_fallback, "mir_key_flag_fallback" }
    };
    for (auto const& entry : key_flag_strings)
    {
        if (flags & entry.flag)
            ss << entry.str << " ";
    }
    return ss.str();
}

std::string format_modifiers (unsigned int modifiers)
{
    std::stringstream ss;

    static struct { MirKeyModifier modifier; char const* str; } const key_modifier_strings[] = {
        { mir_key_modifier_none, "mir_key_modifier_none" },
        { mir_key_modifier_alt, "mir_key_modifier_alt" },
        { mir_key_modifier_alt_left, "mir_key_modifier_alt_left" },
        { mir_key_modifier_alt_right, "mir_key_modifier_alt_right" },
        { mir_key_modifier_shift, "mir_key_modifier_shift" },
        { mir_key_modifier_shift_left, "mir_key_modifier_shift_left" },
        { mir_key_modifier_shift_right, "mir_key_modifier_shift_right" },
        { mir_key_modifier_sym, "mir_key_modifier_sym" },
        { mir_key_modifier_function, "mir_key_modifier_function" },
        { mir_key_modifier_ctrl, "mir_key_modifier_ctrl" },
        { mir_key_modifier_ctrl_left, "mir_key_modifier_ctrl_left" },
        { mir_key_modifier_ctrl_right, "mir_key_modifier_ctrl_right" },
        { mir_key_modifier_meta, "mir_key_modifier_meta" },
        { mir_key_modifier_meta_left, "mir_key_modifier_meta_left" },
        { mir_key_modifier_meta_right, "mir_key_modifier_meta_right" },
        { mir_key_modifier_caps_lock, "mir_key_modifier_caps_lock" },
        { mir_key_modifier_num_lock, "mir_key_modifier_num_lock" },
        { mir_key_modifier_scroll_lock, "mir_key_modifier_scroll_lock" }
    };
    for (auto const& entry : key_modifier_strings)
    {
        if (modifiers & entry.modifier)
            ss << entry.str << " ";
    }
    return ss.str();
}

void format_key_event(std::stringstream &ss, MirKeyEvent const& ev)
{
    ss << "MirKeyEvent {" << std::endl;
    ss << "  device_id: " << ev.device_id << std::endl;
    ss << "  source_id: " << ev.source_id << std::endl;
    ss << "  action: " << format_key_action(ev.action) << std::endl;
    ss << "  flags: " << format_key_flags(ev.flags) << std::endl;
    ss << "  modifiers: " << format_modifiers(ev.modifiers) << std::endl;
    ss << "  key_code: " << ev.key_code << std::endl;
    ss << "  scan_code: " << ev.scan_code << std::endl;
    ss << "  repeat_count: " << ev.repeat_count << std::endl;
    ss << "  down_time: " << ev.down_time << std::endl;
    ss << "  event_time: " << ev.event_time << std::endl;
    ss << "  is_system_key: " << ev.is_system_key << std::endl;
    ss << "}";
}

std::string format_motion_action(int action)
{
    std::stringstream ss;

    static std::string const motion_action_strings[] = {
        "mir_motion_action_down",
        "mir_motion_action_up",
        "mir_motion_action_move",
        "mir_motion_action_cancel",
        "mir_motion_action_outside",
        "mir_motion_action_pointer_down",
        "mir_motion_action_pointer_up",
        "mir_motion_action_hover_move",
        "mir_motion_action_scroll",
        "mir_motion_action_hover_enter",
        "mir_motion_action_hover_exit"
    };

    ss << motion_action_strings[action];

    return ss.str();
}

std::string format_motion_flags(MirMotionFlag flags)
{
    std::stringstream ss;

    if (flags & mir_motion_flag_window_is_obscured)
        ss << "mir_motion_flag_window_is_obscured";
    
    return ss.str();
}

std::string format_button_state(MirMotionButton button_state)
{
    std::stringstream ss;

    static struct { MirMotionButton button; char const* str; } const button_strings[] = {
        { mir_motion_button_primary, "mir_motion_button_primary" },
        { mir_motion_button_secondary, "mir_motion_button_secondary" },
        { mir_motion_button_tertiary, "mir_motion_button_tertiary" },
        { mir_motion_button_back, "mir_motion_button_back" },
        { mir_motion_button_forward, "mir_motion_button_forward" }
    };
    for (auto const& entry : button_strings)
    {
        if (button_state & entry.button)
            ss << entry.str << " ";
    }
    return ss.str();
}

void format_motion_event(std::stringstream &ss, MirMotionEvent const& ev)
{
    ss << "MirMotionEvent{" << std::endl;
    ss << "  type: motion" << std::endl;
    ss << "  device_id: " << ev.device_id << std::endl;
    ss << "  source_id: " << ev.source_id << std::endl;
    ss << "  action: " << format_motion_action(ev.action) << std::endl;
    ss << "  flags: " << format_motion_flags(ev.flags) << std::endl;
    ss << "  modifiers: " << format_modifiers(ev.modifiers) << std::endl;
    ss << "  edge_flags: " << ev.edge_flags << std::endl;
    ss << "  button_state: " << format_button_state(ev.button_state) << std::endl;
    ss << "  x_offset: " << ev.x_offset << std::endl;
    ss << "  y_offset: " << ev.y_offset << std::endl;
    ss << "  x_precision: " << ev.x_precision << std::endl;
    ss << "  y_precision: " << ev.y_precision << std::endl;
    ss << "  down_time: " << ev.down_time << std::endl;
    ss << "  event_time: " << ev.event_time << std::endl;
    ss << "  pointer_count: " << ev.pointer_count << std::endl;
    for (unsigned int i = 0; i < ev.pointer_count; i++) {
        ss << "  pointer[" << i << "]{" << std::endl;
        ss << "    id: " << ev.pointer_coordinates[i].id << std::endl;
        ss << "    raw_x: " << ev.pointer_coordinates[i].raw_x << std::endl;
        ss << "    raw_y: " << ev.pointer_coordinates[i].raw_y << std::endl;
        ss << "    x: " << ev.pointer_coordinates[i].x << std::endl; 
        ss << "    y: " << ev.pointer_coordinates[i].y << std::endl;
        ss << "    touch_minor: " << ev.pointer_coordinates[i].touch_minor << std::endl;
        ss << "    size: " << ev.pointer_coordinates[i].size << std::endl;
        ss << "    pressure: " << ev.pointer_coordinates[i].pressure << std::endl;
        ss << "    orientation: " << ev.pointer_coordinates[i].orientation << std::endl;
        ss << "    vscroll: " << ev.pointer_coordinates[i].vscroll << std::endl;
        ss << "    hscroll: " << ev.pointer_coordinates[i].hscroll << std::endl;
        ss << "  }" << std::endl;
    }
    ss << "}" << std::endl;
    
}

void format_event(std::stringstream &ss, MirEvent const& ev)
{
    switch (ev.type)
    {
    case mir_event_type_key:
        format_key_event(ss, ev.key);
        break;
    case mir_event_type_motion:
        format_motion_event(ss, ev.motion);
        break;
    default:
        BOOST_THROW_EXCEPTION(std::runtime_error("Unexpected event type"));
    }
}

}

void mcll::InputReceiverReport::received_event(
    MirEvent const& event)
{
    std::stringstream ss;

    ss << "Received event:" << std::endl;
    
    format_event(ss, event);

    logger->log<ml::Logger::debug>(ss.str(), component);
}
