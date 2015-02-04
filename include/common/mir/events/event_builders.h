/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Robert Carr <robert.carr@canonical.com>
 */

#define MIR_INCLUDE_DEPRECATED_EVENT_HEADER

#ifndef MIR_EVENT_BUILDERS_H_
#define MIR_EVENT_BUILDERS_H_

#include "mir_toolkit/event.h"

#include "mir/geometry/size.h"
#include "mir/frontend/surface_id.h"

#include <memory>
#include <vector>
#include <functional>

namespace mir
{
    typedef std::unique_ptr<MirEvent, void(*)(MirEvent*)> EventUPtr;
    
namespace events
{
// Surface orientation change event
EventUPtr make_event(frontend::SurfaceId const& surface_id, MirOrientation orientation);
// Prompt session state change event
EventUPtr make_event(MirPromptSessionState state);
// Surface resize event
EventUPtr make_event(frontend::SurfaceId const& surface_id, geometry::Size const& size);
// Surface configure event
EventUPtr make_event(frontend::SurfaceId const& surface_id, MirSurfaceAttrib attribute, int value);
// Close surface event
EventUPtr make_event(frontend::SurfaceId const& surface_id);

// Key event
EventUPtr make_event(MirInputDeviceId device_id, int64_t timestamp,
    MirKeyInputAction action, xkb_keysym_t key_code,
    int scan_code, MirInputModifiers modifiers);

// Touch event
EventUPtr make_event(MirInputDeviceId device_id, int64_t timestamp,
    MirInputModifiers modifiers);
void add_touch(MirEvent &event, MirTouchInputTouchId touch_id, MirTouchInputTouchAction action,
    MirTouchInputTouchTooltype tooltype, float x_axis_value, float y_axis_value,
    float pressure_value, float touch_major_value, float touch_minor_value, float size_value);

// Pointer event
EventUPtr make_event(MirInputDeviceId device_id, int64_t timestamp,
    MirInputModifiers modifiers, MirPointerInputAction action,
    std::vector<MirPointerInputButton> const& buttons_pressed,
    float x_axis_value, float y_axis_value,
    float hscroll_value, float vscroll_value);
}
}

#endif // MIR_EVENT_BUILDERS_H_
