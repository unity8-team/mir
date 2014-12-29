/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TOOLKIT_EVENT_H_
#define MIR_TOOLKIT_EVENT_H_

#include <stddef.h>
#include <stdint.h>
#include "mir_toolkit/common.h"

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

typedef enum
{
    mir_event_type_key,
    mir_event_type_motion,
    mir_event_type_surface,
    mir_event_type_resize,
    mir_event_type_prompt_session_state_change,
    mir_event_type_orientation,
    mir_event_type_close_surface,
    /* Type for new style input event will be returned from mir_event_get_type
       when old style event type was mir_event_type_key or mir_event_type_motion */
    mir_event_type_input
} MirEventType;

typedef struct MirSurfaceEvent MirSurfaceEvent;
typedef struct MirResizeEvent MirResizeEvent;
typedef struct MirPromptSessionEvent MirPromptSessionEvent;
typedef struct MirOrientationEvent MirOrientationEvent;
typedef struct MirCloseSurfaceEvent MirCloseSurfaceEvent;
typedef struct MirInputEvent MirInputEvent;

typedef union MirEvent MirEvent;

#ifdef __cplusplus
}
/**@}*/
#endif

#include "mir_toolkit/events/event_deprecated.h"

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

/*
 * Retrieves the type of a MirEvent. Now preferred over direct access to ev->type.
 * In particular ev->type will never be mir_event_type_input and mir_event_get_type
 * is the only way to ensure mir_event_get_input_event will succeed.
 *
 * \param [in] event The event
 * \return           The event type
 */
MirEventType mir_event_get_type(MirEvent const* ev);

/*
 * Retrieve the MirInputEvent associated with a MirEvent of 
 * type mir_event_type_input. See <mir_toolkit/events/input/input_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirInputEvent
 */
MirInputEvent const* mir_event_get_input_event(MirEvent const* ev);

/*
 * Retrieve the MirSurfaceEvent associated with a MirEvent of
 * type mir_event_type_surface. See <mir_toolkit/events/surface_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirSurfaceEvent
 */
MirSurfaceEvent const* mir_event_get_surface_event(MirEvent const* ev);

/*
 * Retrieve the MirResizeEvent associated with a MirEvent of
 * type mir_event_type_resize. See <mir_toolkits/events/resize_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirResizeEvent
 */
MirResizeEvent const* mir_event_get_resize_event(MirEvent const* ev);

/*
 * Retrieve the MirPromptSessionEvent associated with a MirEvent of
 * type mir_event_type_prompt_session_state_change. See <mir_toolkits/events/prompt_session_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirPromptSessionEvent
 */
MirPromptSessionEvent const* mir_event_get_prompt_session_event(MirEvent const* ev);

/*
 * Retrieve the MirOrientationEvent associated with a MirEvent of
 * type mir_event_type_orientation. See <mir_toolkit/events/orientation_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirOrientationEvent
 */
MirOrientationEvent const* mir_event_get_orientation_event(MirEvent const* ev);

/*
 * Retrieve the MirCloseSurfaceEvent associated with a MirEvent of
 * type mir_event_type_close_surface. See <mir_toolkit/events/close_surface_event.h>
 * for accessors.
 *
 * \param [in] event The event
 * \return           The associated MirCloseSurfaceEvent
 */
MirCloseSurfaceEvent const* mir_event_get_close_surface_event(MirEvent const* ev);

/*
 * 
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * _________________________
 *< Don't use mir_event_ref >
 *-------------------------
 *       \   ^__^
 *        \  (oo)\_______
*            (__)\       )\/\
 *                ||----w |
 *                ||     || 
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTICE: mir_event_ref and mir_event_unref are implemented in terms of copy until 
 * such point whereas direct MirEvent access as deprecated. This means you must
 * use the return value as your new reference 
 *
 */

/*
 * Reference this MirEvent and return a pointer to the
 * newly referenced instance
 *
 * \param[in] The event to reference
 * \return    The event pointer to now use
 */
MirEvent const* mir_event_ref(MirEvent const* ev) __attribute__((warn_unused_result));

/*
 * Release a reference to a MirEvent.
 *
 * \param[in] The event to un-reference
 */
void mir_event_unref(MirEvent const* ev);

#ifdef __cplusplus
}
/**@}*/
#endif

#endif /* MIR_TOOLKIT_EVENT_H_ */
