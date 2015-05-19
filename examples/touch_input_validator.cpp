/*
 * Client which validates incoming touch events.
 *
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
 * Author: Robert Carr <robert.carr@canonical.com>
 */

#include "eglapp.h"

#include <mir_toolkit/mir_client_library.h>

#include <GLES2/gl2.h>

#include <set>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <array>

namespace
{
// Validates the state transition between two MirTouchevents is valid.
//    
// Assumes all touch input comes from a single device.
// Validates:
//     1. All touches which were down stay down unless they were coming up
//     2. All touches which were released do not appear
//     3. No touches appear before a down
//     4. Only one touch comes up or down per event

bool validate_events(MirTouchEvent const* previous_event, MirTouchEvent const* current_event)
{
    std::set<MirTouchId> must_be_present;
    std::set<MirTouchId> may_not_be_present;
    
    for (size_t i = 0; i < mir_touch_event_point_count(previous_event); i++)
    {
        auto id = mir_touch_event_id(previous_event, i);
        auto action = mir_touch_event_action(previous_event, i);
        if (action == mir_touch_action_change)
            must_be_present.insert(id);
        else if (action == mir_touch_action_down)
            must_be_present.insert(id);
        else
            may_not_be_present.insert(id);
    }
    if (may_not_be_present.size() > 1)
    {
        printf("More than one touch came up\n");
        return false;
    }

    bool found_a_up_down = false;
    for (size_t i = 0; i < mir_touch_event_point_count(current_event); i++)
    {
        auto id = mir_touch_event_id(current_event, i);
        auto it = may_not_be_present.find(id);
        if (it != may_not_be_present.end())
        {
            printf("We repeated a touch which was released (%d)\n", static_cast<int>(id));
            return false;
        }
        it = must_be_present.find(id);
        if (it != must_be_present.end())
            must_be_present.erase(it);
        else if (mir_touch_event_action(current_event, i) == mir_touch_action_down)
        {
            if (found_a_up_down)
                printf("Found too many downs in one event\n");
            found_a_up_down = true;
        }
        if (mir_touch_event_action(current_event, i) == mir_touch_action_up)
        {
            if (found_a_up_down)
                printf("Found too many ups in one event\n");
            found_a_up_down = true;
        }
    }

    if (must_be_present.size())
    {
        printf("We received a touch which did not contain all required IDs\n");
        return false;
    }
    
    return true;
}
    
class TouchVerifier
{
public:
    TouchVerifier() : previous_event(nullptr) {}
    ~TouchVerifier() { if (previous_event) mir_event_unref(previous_event); }
    
    void record_event(MirEvent const* event)
    {
	if (mir_event_get_type(event) != mir_event_type_input)
	    return;
	auto iev = mir_event_get_input_event(event);
	if (mir_input_event_get_type(iev) != mir_input_event_type_touch)
	    return;
	auto tev = mir_input_event_get_touch_event(iev);
				     
        if (!previous_event)
        {
            previous_event = mir_event_ref(event);
            return;
        }
        if (!validate_events(mir_input_event_get_touch_event(
	    mir_event_get_input_event(previous_event)), tev))
            abort();
        
        mir_event_unref(previous_event);
        previous_event = mir_event_ref(event);
    }
private:
    MirEvent const* previous_event;
};

void on_event(MirSurface * /*surface*/, const MirEvent *event, void *context)
{
    auto state = reinterpret_cast<TouchVerifier*>(context);

    state->record_event(event);
}
}


typedef struct Color
{
    GLfloat r, g, b, a;
} Color;

int main(int argc, char *argv[])
{
    unsigned int width = 0, height = 0;

    if (!mir_eglapp_init(argc, argv, &width, &height))
        return 1;

    TouchVerifier state;

    MirSurface *surface = mir_eglapp_native_surface();
    mir_surface_set_event_handler(surface, on_event, &state);

    float const opacity = mir_eglapp_background_opacity;
    std::array<Color, 3> colors{{Color{opacity, 0.0f, 0.0f, opacity},
        Color{0.0f, opacity, 0.0f, opacity},
        Color{0.0f, 0.0f, opacity, opacity}}};

    /* This is probably the simplest GL you can do */
    while (mir_eglapp_running())
    {
	for (auto const& c : colors)
	{
	    glClearColor(c.r, c.g, c.b, c.a);
	    glClear(GL_COLOR_BUFFER_BIT);
	    mir_eglapp_swap_buffers();
	    sleep(1);
	}
    }

    mir_eglapp_shutdown();

    return 0;
}
