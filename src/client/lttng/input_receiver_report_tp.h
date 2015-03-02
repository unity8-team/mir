/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER mir_client_input_receiver

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./input_receiver_report_tp.h"

#if !defined(MIR_CLIENT_LTTNG_INPUT_RECEIVER_REPORT_TP_H_) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define MIR_CLIENT_LTTNG_INPUT_RECEIVER_REPORT_TP_H_

#include <lttng/tracepoint.h>
#include <stdint.h>

TRACEPOINT_EVENT(
    mir_client_input_receiver,
    key_event,
    TP_ARGS(int32_t, device_id, int, action, unsigned int, modifiers, int32_t, key_code, int32_t, scan_code, int64_t, event_time),
    TP_FIELDS(
        ctf_integer(int32_t, device_id, device_id)
        ctf_integer(int, action, action)
        ctf_integer(unsigned int, modifiers, modifiers)
        ctf_integer(int32_t, key_code, key_code)
        ctf_integer(int32_t, scan_code, scan_code)
        ctf_integer(int64_t, event_time, event_time)
    )
)

TRACEPOINT_EVENT(
    mir_client_input_receiver,
    touch_event,
    TP_ARGS(int32_t, device_id, unsigned int, modifiers, int64_t, event_time),
    TP_FIELDS(
        ctf_integer(int32_t, device_id, device_id)
        ctf_integer(unsigned int, modifiers, modifiers)
        ctf_integer(int64_t, event_time, event_time)
    )
)

TRACEPOINT_EVENT(
    mir_client_input_receiver,
    touch_event_coordinate,
    TP_ARGS(int, id, float, x, float, y, float, touch_major, float, touch_minor, float, size, float, pressure),
    TP_FIELDS(
        ctf_integer(int, id, id)
        ctf_float(float, x, x)
        ctf_float(float, y, y)
        ctf_float(float, touch_major, touch_major)
        ctf_float(float, touch_minor, touch_minor)
        ctf_float(float, size, size)
        ctf_float(float, pressure, pressure)
    )
)

#endif /* MIR_CLIENT_LTTNG_INPUT_RECEIVER_REPORT_TP_H_ */

#include <lttng/tracepoint-event.h>
