/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_INPUT_EVENT_H_
#define UBUNTU_APPLICATION_UI_INPUT_EVENT_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    static const size_t UBUNTU_APPLICATION_UI_INPUT_EVENT_MAX_POINTER_COUNT = 16;

    typedef int64_t nsecs_t;

    typedef enum
    {
        KEY_EVENT_TYPE,
        MOTION_EVENT_TYPE,
        HW_SWITCH_EVENT_TYPE
    } EventType;

    struct Event
    {
        // Generic event properties
        EventType type;
        int32_t device_id;
        int32_t source_id;
        int32_t action;
        int32_t flags;
        int32_t meta_state;
        // Information specific to key/motion event types
        union
        {
            struct HardwareSwitchEvent
            {
                nsecs_t event_time;
                uint32_t policy_flags;
                int32_t switch_code;
                int32_t switch_value;
            } hw_switch;
            struct KeyEvent
            {
                int32_t key_code;
                int32_t scan_code;
                int32_t repeat_count;
                nsecs_t down_time;
                nsecs_t event_time;
                bool is_system_key;
            } key;
            struct MotionEvent
            {
                int32_t edge_flags;
                int32_t button_state;
                float x_offset;
                float y_offset;
                float x_precision;
                float y_precision;
                nsecs_t down_time;
                nsecs_t event_time;

                size_t pointer_count;
                struct PointerCoordinates
                {
                    int id;
                    float x, raw_x;
                    float y, raw_y;
                    float touch_major;
                    float touch_minor;
                    float size;
                    float pressure;
                    float orientation;
                };
                PointerCoordinates pointer_coordinates[UBUNTU_APPLICATION_UI_INPUT_EVENT_MAX_POINTER_COUNT];
            } motion;
        } details;
    };

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_UI_INPUT_EVENT_H_
