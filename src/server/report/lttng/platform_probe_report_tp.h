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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER mir_server_platform_probe

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./platform_probe_report_tp.h"

#if !defined(MIR_LTTNG_PLATFORM_PROBE_REPORT_TP_H_) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define MIR_LTTNG_PLATFORM_PROBE_REPORT_TP_H_

#include "lttng_utils.h"

MIR_LTTNG_VOID_TRACE_CLASS(mir_server_platform_probe)

TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    module_probed,
    TP_ARGS(char const*, name, int, priority),
    TP_FIELDS(ctf_string(name, name), ctf_integer(int, priority, priority))
)

TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    invalid_module_probed,
    TP_ARGS(char const*, error),
    TP_FIELDS(ctf_string(name, name))
)

TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    module_selected,
    TP_ARGS(char const*, name),
    TP_FIELDS(ctf_string(name, name))
)

#include "lttng_utils_pop.h"

#endif /* MIR_LTTNG_COMPOSITOR_REPORT_TP_H_ */

#include <lttng/tracepoint-event.h>
