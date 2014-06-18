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

#include <ubuntu/application/ui/session.h>

// TODO<papi>: Not sure what this should do for mir. ApplicationInstance maps to the mir
// concept of a session and this doesn't really seem to be used

extern "C"
{

UAUiSession* ua_ui_session_new_with_properties(UAUiSessionProperties *properties)
{
    return nullptr;
}
    
UAUiSessionProperties* ua_ui_session_properties_new()
{
    return nullptr;
}
   
void ua_ui_session_properties_set_type(UAUiSessionProperties* properties,
    UAUiSessionType type)
{
    (void) properties;
    (void) type;
}

void ua_ui_session_properties_set_remote_pid(UAUiSessionProperties* properties,
                                             uint32_t pid)
{
    (void) properties;
    (void) pid;
}
                                             
}
