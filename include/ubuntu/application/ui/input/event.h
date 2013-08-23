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

#include <ubuntu/visibility.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO<papi>: Perhaps we should update the struct to use these enum types. But then we need to move action/flags
// in to the internal typed event structs.

typedef enum {
    U_KEY_ACTION_DOWN     = 0,
    U_KEY_ACTION_UP       = 1,
    U_KEY_ACTION_MULTIPLE = 2
} UKeyAction;

typedef enum {
    U_KEY_FLAG_WOKE_HERE           = 0x1,
    U_KEY_FLAG_SOFT_KEYBOARD       = 0x2,
    U_KEY_FLAG_KEEP_TOUCH_MODE     = 0x4,
    U_KEY_FLAG_FROM_SYSTEM         = 0x8,
    U_KEY_FLAG_EDITOR_ACTION       = 0x10,
    U_KEY_FLAG_CANCELED            = 0x20,
    U_KEY_FLAG_VIRTUAL_HARD_KEY    = 0x40,
    U_KEY_FLAG_LONG_PRESS          = 0x80,
    U_KEY_FLAG_CANCELED_LONG_PRESS = 0x100,
    U_KEY_FLAG_TRACKING            = 0x200,
    U_KEY_FLAG_FALLBACK            = 0x400
} UKeyFlag;

typedef enum {
    U_KEY_MODIFIER_NONE        = 0,
    U_KEY_MODIFIER_ALT         = 0x02,
    U_KEY_MODIFIER_ALT_LEFT    = 0x10,
    U_KEY_MODIFIER_ALT_RIGHT   = 0x20,
    U_KEY_MODIFIER_SHIFT       = 0x01,
    U_KEY_MODIFIER_SHIFT_LEFT  = 0x40,
    U_KEY_MODIFIER_SHIFT_RIGHT = 0x80,
    U_KEY_MODIFIER_SYM         = 0x04,
    U_KEY_MODIFIER_FUNCTION    = 0x08,
    U_KEY_MODIFIER_CTRL        = 0x1000,
    U_KEY_MODIFIER_CTRL_LEFT   = 0x2000,
    U_KEY_MODIFIER_CTRL_RIGHT  = 0x4000,
    U_KEY_MODIFIER_META        = 0x10000,
    U_KEY_MODIFIER_META_LEFT   = 0x20000,
    U_KEY_MODIFIER_META_RIGHT  = 0x40000,
    U_KEY_MODIFIER_CAPS_LOCK   = 0x100000,
    U_KEY_MODIFIER_NUM_LOCK    = 0x200000,
    U_KEY_MODIFIER_SCROLL_LOCK = 0x400000
} UKeyModifierMask;

#define U_MOTION_ACTION_POINTER_INDEX_SHIFT 8
#define U_MOTION_ACTION_MASK 0xff
#define U_MOTION_ACTION_POINTER_INDEX_MASK 0xff00

typedef enum {
    U_MOTION_ACTION_DOWN         = 0,
    U_MOTION_ACTION_UP           = 1,
    U_MOTION_ACTION_MOVE         = 2,
    U_MOTION_ACTION_CANCEL       = 3,
    U_MOTION_ACTION_OUTSIDE      = 4,
    U_MOTION_ACTION_POINTER_DOWN = 5,
    U_MOTION_ACTION_POINTER_UP   = 6,
    U_MOTION_ACTION_HOVER_MOVE   = 7,
    U_MOTION_ACTION_SCROLL       = 8,
    U_MOTION_ACTION_HOVER_ENTER  = 9,
    U_MOTION_ACTION_HOVER_EXIT   = 10
} UMotionAction;

typedef enum {
    U_MOTION_FLAG_WINDOW_IS_OBSCURED = 0x1
} UMotionFlag;

typedef enum {
    U_MOTION_BUTTON_PRIMARY   = 1 << 0,
    U_MOTION_BUTTON_SECONDARY = 1 << 1,
    U_MOTION_BUTTON_TERTIARY  = 1 << 2,
    U_MOTION_BUTTON_BACK      = 1 << 3,
    U_MOTION_BUTTON_FORWARD   = 1 << 4
} UMotionButtonMask;


    /** Maximum number of pointers reported within one input event. */
#define UBUNTU_APPLICATION_UI_INPUT_EVENT_MAX_POINTER_COUNT (16)

    /** Reference timebase, nanoseconds as measured by CLOCK_MONOTONIC. */
    typedef int64_t nsecs_t;

    /** Describes the different event types. */
    typedef enum
    {
        KEY_EVENT_TYPE, ///< Event originates from a keyboard.
        MOTION_EVENT_TYPE, ///< Event originates from something moving, e.g., a wheel, a mouse, a finger on a touchpad.
        HW_SWITCH_EVENT_TYPE ///< Event originates from an additional button attached to the device's HW, e.g., power button.
    } EventType;

    /** Models an input event. */
    typedef struct
    {
        EventType type; ///< Type of the event.
        int32_t device_id; ///< Device that this event originated from.
        int32_t source_id; ///< Source that this event originated from.
        int32_t action; ///< Action signalled by this event.
        int32_t flags; ///< Flags associated with this event.
        int32_t meta_state; ///< State of the meta modifiers (ALT, CTRL, SHIFT).
        /** Information specific to key/motion event types. */
        union
        {
            /** Information describing an event originating from a HW switch. */
            struct HardwareSwitchEvent
            {
                nsecs_t event_time; ///< Timestamp when the event happened.
                uint32_t policy_flags; ///< Policy flags.
                int32_t switch_code; ///< The scan code of the switch.
                int32_t switch_value; ///< The value reported by the switch.
            } hw_switch;
            /** Information describing an event originating from a keyboard key. */
            struct KeyEvent
            {
                int32_t key_code; 
                int32_t scan_code;
                int32_t repeat_count;
                nsecs_t down_time;
                nsecs_t event_time;
                int is_system_key;  ///< \deprecated Do not use.
            } key;
            /** Information describing an event originating from a
             * "moving" device, e.g., a mouse, a mouse-wheel, a finger
             * on a touchpad. 
             */
            struct MotionEvent
            {
                int32_t edge_flags; ///< Set for touches intersecting a touchscreen's edges, requires HW support.
                int32_t button_state; ///< State of buttons of the device
                float x_offset; ///< Movement in x direction since down event
                float y_offset; ///< Movement in y direction since down event
                float x_precision; ///< Sampling precision in x direction
                float y_precision; ///< Sampling precision in y direction
                nsecs_t down_time; ///< Timestamp that marks the down event
                nsecs_t event_time; ///< Timestamp that marks when this event happened
                size_t pointer_count; ///< Number of pointers reported in this event
                struct PointerCoordinate
                {
                    int id; ///< Unique id of the pointer
                    float x, raw_x; ///< Processed and raw x coordinates
                    float y, raw_y; ///< Processed and raw y coordinates
                    float touch_major; ///< Touch major coordinate
                    float touch_minor; ///< Touch minor coordinate
                    float size; ///< Size of the pointer/touch
                    float pressure; ///< Pressure of the touch
                    float orientation; ///< Orientation
                } pointer_coordinates[UBUNTU_APPLICATION_UI_INPUT_EVENT_MAX_POINTER_COUNT]; ///< Pointer information, valid from [0,pointer_count).
            } motion;
        } details;
    } Event;

#ifdef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_UI_INPUT_EVENT_H_
