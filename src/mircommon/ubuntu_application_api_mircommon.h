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

#ifndef UBUNTU_APPLICATION_API_MIR_COMMON_H_
#define UBUNTU_APPLICATION_API_MIR_COMMON_H_

#include <ubuntu/application/ui/input/event.h>
#include <mir_toolkit/event.h>

#ifndef __cplusplus
extern "C" {
#endif

void mir_event_to_ubuntu_event(MirEvent const* mir_event, Event& ubuntu_ev);

#ifndef __cplusplus
}
#endif

#endif // UBUNTU_APPLICATION_API_MIR_COMMON_H_
