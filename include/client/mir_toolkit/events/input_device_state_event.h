/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TOOLKIT_EVENTS_INPUT_DEVICE_STATE_EVENT_H_
#define MIR_TOOLKIT_EVENTS_INPUT_DEVICE_STATE_EVENT_H_

#include <mir_toolkit/events/event.h>

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

/**
 * MirInputDeviceStateEvent informs clients about the current state of the
 * input devices. This is necessary when the client did not receive the most
 * recent input events. This happens when the server was resumed after a pause,
 * or when the client just received the input focus
 *
 * The event contains a single pointer button state and the current cursor
 * position. Additionally for key and pointer devices the pressed keys and
 * buttons are supplied individually.
 */

/**
 * Retrieve the button state.
 *
 * \param[in] ev The input device state event
 * \return       The cumulative state of all modifiers
 */
MirPointerButtons mir_input_device_state_event_pointer_buttons(
    MirInputDeviceStateEvent const* ev);

/**
 * Retrieve the pointer position
 *
 * \param[in] ev   The input device state event
 * \param[in] axis The pointer axis: mir_pointer_axis_x or mir_pointer_axis_y
 * \return         The pointer position
 */
float mir_input_device_state_event_pointer_axis(
    MirInputDeviceStateEvent const* ev, MirPointerAxis axis);

/**
 * Retrieve the time associated with a MirInputDeviceStateEvent
 *
 * \param[in] ev The input device state event
 * \return       The time in nanoseconds since epoch
 */
int64_t mir_input_device_state_event_time(
    MirInputDeviceStateEvent const* ev);

/**
 * Retrieve the number of attached input devices.
 *
 * \param[in] ev The input device state event
 * \return       The time in nanoseconds since epoch
 */
uint32_t mir_input_device_state_event_device_count(
    MirInputDeviceStateEvent const* ev);

/**
 * Retrieve the device id 
 *
 * \param[in] ev The input device state event
 * \param[in] index The index of the input device id
 * \return       The device id
 */
MirInputDeviceId mir_input_device_state_event_device_id(
    MirInputDeviceStateEvent const* ev, uint32_t index);

/**
 * Number of keys pressed on the device identified by the \a index.
 *
 * \param[in] ev The input device state event
 * \param[in] index The index of the input device id
 * \return       The input event modifiers
 */
uint32_t const* mir_input_device_state_event_device_pressed_keys(
    MirInputDeviceStateEvent const* ev, uint32_t index);

/**
 * Number of keys pressed on the device identified by the \a index.
 *
 * \param[in] ev The input device state event
 * \param[in] index The index of the input device id
 * \return       The input event modifiers
 */
uint32_t mir_input_device_state_event_device_pressed_keys_count(
    MirInputDeviceStateEvent const* ev, uint32_t index);

/**
 * Retrieve the pointer button state of the device
 *
 * \param[in] ev The input device state event
 * \param[in] index The index of the input device id
 * \return       The pointer button state of the device
 */
MirPointerButtons mir_input_device_state_event_device_pointer_buttons(
    MirInputDeviceStateEvent const* ev, uint32_t index);

#ifdef __cplusplus
}
/**@}*/
#endif

#endif /* MIR_TOOLKIT_EVENTS_INPUT_DEVICE_STATE_EVENT_H_ */
