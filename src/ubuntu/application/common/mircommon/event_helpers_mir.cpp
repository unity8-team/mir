/*
 * Copyright (C) 2013 Canonical Ltd
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

#include "event_helpers_mir.h"

namespace uaum = ubuntu::application::ui::mir;

// TODO<mir>: This begs the question: Why does MirEvent exist? It's difficult to ensure this function is kept in sync
// at the unit test level.
bool
uaum::event_to_ubuntu_event(MirEvent const* mir_event, WindowEvent& ubuntu_ev)
{
    switch (mir_event->type)
    {
    case mir_event_type_key:
        ubuntu_ev.key.type = KEY_WEVENT_TYPE;
        ubuntu_ev.key.device_id = mir_event->key.device_id;
        ubuntu_ev.key.source_id = mir_event->key.source_id;
        ubuntu_ev.key.action = mir_event->key.action;
        ubuntu_ev.key.flags = mir_event->key.flags;
        ubuntu_ev.key.meta_state = mir_event->key.modifiers;
        ubuntu_ev.key.key_code = mir_event->key.key_code;
        ubuntu_ev.key.scan_code = mir_event->key.scan_code;
        ubuntu_ev.key.repeat_count = mir_event->key.repeat_count;
        ubuntu_ev.key.down_time = mir_event->key.down_time;
        ubuntu_ev.key.event_time = mir_event->key.event_time;
        ubuntu_ev.key.is_system_key = mir_event->key.is_system_key;
        return true;
    case mir_event_type_motion:
        ubuntu_ev.motion.type = MOTION_WEVENT_TYPE;
        ubuntu_ev.motion.device_id = mir_event->motion.device_id;
        ubuntu_ev.motion.source_id = mir_event->motion.source_id;
        ubuntu_ev.motion.action = mir_event->motion.action;
        ubuntu_ev.motion.flags = mir_event->motion.flags;
        ubuntu_ev.motion.meta_state = mir_event->motion.modifiers;
        ubuntu_ev.motion.edge_flags = mir_event->motion.edge_flags;
        ubuntu_ev.motion.button_state = mir_event->motion.button_state;
        ubuntu_ev.motion.x_offset = mir_event->motion.x_offset;
        ubuntu_ev.motion.y_offset = mir_event->motion.y_offset;
        ubuntu_ev.motion.x_precision = mir_event->motion.x_precision;
        ubuntu_ev.motion.y_precision = mir_event->motion.y_precision;
        ubuntu_ev.motion.down_time = mir_event->motion.down_time;
        ubuntu_ev.motion.event_time = mir_event->motion.event_time;
        ubuntu_ev.motion.pointer_count = mir_event->motion.pointer_count;
        for (int i = 0; i < mir_event->motion.pointer_count; i++)
        {
            ubuntu_ev.motion.pointer_coordinates[i].id = mir_event->motion.pointer_coordinates[i].id;
            ubuntu_ev.motion.pointer_coordinates[i].x = mir_event->motion.pointer_coordinates[i].x;
            ubuntu_ev.motion.pointer_coordinates[i].raw_x = mir_event->motion.pointer_coordinates[i].raw_x;
            ubuntu_ev.motion.pointer_coordinates[i].y = mir_event->motion.pointer_coordinates[i].y;
            ubuntu_ev.motion.pointer_coordinates[i].raw_y = mir_event->motion.pointer_coordinates[i].raw_y;
            ubuntu_ev.motion.pointer_coordinates[i].touch_major = mir_event->motion.pointer_coordinates[i].touch_major;
            ubuntu_ev.motion.pointer_coordinates[i].touch_minor = mir_event->motion.pointer_coordinates[i].touch_minor;
            ubuntu_ev.motion.pointer_coordinates[i].size = mir_event->motion.pointer_coordinates[i].size;
            ubuntu_ev.motion.pointer_coordinates[i].pressure = mir_event->motion.pointer_coordinates[i].pressure;
            ubuntu_ev.motion.pointer_coordinates[i].orientation = mir_event->motion.pointer_coordinates[i].orientation;
        }
        return true;
    case mir_event_type_resize:
        ubuntu_ev.resize.type = RESIZE_WEVENT_TYPE;
        ubuntu_ev.resize.width = mir_event->resize.width;
        ubuntu_ev.resize.height = mir_event->resize.height;
        return true;
    case mir_event_type_surface:
        ubuntu_ev.surface.type = SURFACE_WEVENT_TYPE;
        if (mir_event->surface.attrib == mir_surface_attrib_focus) {
            ubuntu_ev.surface.attribute = SURFACE_ATTRIBUTE_FOCUS;
            ubuntu_ev.surface.value = mir_event->surface.value == mir_surface_focused;
            return true;
        } else {
            return false;
        }
    default:
        return false;
    }
}

bool
uaum::event_to_ubuntu_deprecated_event(MirEvent const* mir_event, Event& ubuntu_ev)
{
    switch (mir_event->type)
    {
    case mir_event_type_key:
        ubuntu_ev.type = KEY_EVENT_TYPE;
        ubuntu_ev.device_id = mir_event->key.device_id;
        ubuntu_ev.source_id = mir_event->key.source_id;
        ubuntu_ev.action = mir_event->key.action;
        ubuntu_ev.flags = mir_event->key.flags;
        ubuntu_ev.meta_state = mir_event->key.modifiers;
        ubuntu_ev.details.key.key_code = mir_event->key.key_code;
        ubuntu_ev.details.key.scan_code = mir_event->key.scan_code;
        ubuntu_ev.details.key.repeat_count = mir_event->key.repeat_count;
        ubuntu_ev.details.key.down_time = mir_event->key.down_time;
        ubuntu_ev.details.key.event_time = mir_event->key.event_time;
        ubuntu_ev.details.key.is_system_key = mir_event->key.is_system_key;
        
        return true;
    case mir_event_type_motion:
        ubuntu_ev.type = MOTION_EVENT_TYPE;
        ubuntu_ev.device_id = mir_event->motion.device_id;
        ubuntu_ev.source_id = mir_event->motion.source_id;
        ubuntu_ev.action = mir_event->motion.action;
        ubuntu_ev.flags = mir_event->motion.flags;
        ubuntu_ev.meta_state = mir_event->motion.modifiers;
        ubuntu_ev.details.motion.edge_flags = mir_event->motion.edge_flags;
        ubuntu_ev.details.motion.button_state = mir_event->motion.button_state;
        ubuntu_ev.details.motion.x_offset = mir_event->motion.x_offset;
        ubuntu_ev.details.motion.y_offset = mir_event->motion.y_offset;
        ubuntu_ev.details.motion.x_precision = mir_event->motion.x_precision;
        ubuntu_ev.details.motion.y_precision = mir_event->motion.y_precision;
        ubuntu_ev.details.motion.down_time = mir_event->motion.down_time;
        ubuntu_ev.details.motion.event_time = mir_event->motion.event_time;
        ubuntu_ev.details.motion.pointer_count = mir_event->motion.pointer_count;
        for (int i = 0; i < mir_event->motion.pointer_count; i++)
        {
            ubuntu_ev.details.motion.pointer_coordinates[i].id = mir_event->motion.pointer_coordinates[i].id;
            ubuntu_ev.details.motion.pointer_coordinates[i].x = mir_event->motion.pointer_coordinates[i].x;
            ubuntu_ev.details.motion.pointer_coordinates[i].raw_x = mir_event->motion.pointer_coordinates[i].raw_x;
            ubuntu_ev.details.motion.pointer_coordinates[i].y = mir_event->motion.pointer_coordinates[i].y;
            ubuntu_ev.details.motion.pointer_coordinates[i].raw_y = mir_event->motion.pointer_coordinates[i].raw_y;
            ubuntu_ev.details.motion.pointer_coordinates[i].touch_major = mir_event->motion.pointer_coordinates[i].touch_major;
            ubuntu_ev.details.motion.pointer_coordinates[i].touch_minor = mir_event->motion.pointer_coordinates[i].touch_minor;
            ubuntu_ev.details.motion.pointer_coordinates[i].size = mir_event->motion.pointer_coordinates[i].size;
            ubuntu_ev.details.motion.pointer_coordinates[i].pressure = mir_event->motion.pointer_coordinates[i].pressure;
            ubuntu_ev.details.motion.pointer_coordinates[i].orientation = mir_event->motion.pointer_coordinates[i].orientation;
        }
        return true;
    default:
        return false;
    }
}
