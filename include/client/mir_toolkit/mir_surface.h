/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 */

#ifndef MIR_TOOLKIT_MIR_SURFACE_H_
#define MIR_TOOLKIT_MIR_SURFACE_H_

#include <mir_toolkit/mir_native_buffer.h>
#include <mir_toolkit/client_types.h>
#include <mir_toolkit/common.h>
#include <mir_toolkit/mir_cursor_configuration.h>

#include <stdbool.h>

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

/**
 * Create a surface specification for a normal surface.
 *
 * A normal surface is suitable for most application windows. It has no special semantics.
 * On a desktop shell it will typically have a title-bar, be movable, resizeable, etc.
 *
 * \param [in] connection   Connection the surface will be created on
 * \param [in] width        Requested width. The server is not guaranteed to return a surface of this width.
 * \param [in] height       Requested height. The server is not guaranteed to return a surface with this height.
 * \param [in] format       Pixel format for the surface.
 * \return                  A handle that can be passed to mir_surface_realise to complete construction.
 */
MirSurfacePlan* mir_connection_create_plan_for_normal_surface(MirConnection* connection,
                                                              int width,
                                                              int height,
                                                              MirPixelFormat format);

/**
 * Create a surface from a given specification
 * \param [in] requested_specification  Specification of the attributes for the created surface
 * \param [in] callback                 Callback function to be invoked when realisation is complete
 * \param [in, out] context             User data passed to callback function.
 * \return                              A handle that can be passed to mir_wait_for
 * \note    This consumes requested_specification. No further calls should be made with it.
 */
MirWaitHandle* mir_surface_realise(MirSurfacePlan* requested_specification,
                                   mir_surface_callback callback, void* context);

/**
 * Create a surface from a given specification and wait for the result.
 * \param [in] requested_specification  Specification of the attributes for the created surface
 * \return                              The new surface. This is guaranteed non-null, but may be invalid
 *                                      in the case of error.
 * \note    This consumes requested_specification. No further calls should be made with it.
 */
MirSurface* mir_surface_realise_sync(MirSurfacePlan* requested_specification);

/**
 * Set the requested name.
 *
 * The surface name helps the user to distinguish between multiple surfaces
 * from the same application. A typical desktop shell may use it to provide
 * text in the window titlebar, in an alt-tab switcher, or equivalent.
 *
 * \param [in] spec     Specification to mutate
 * \param [in] name     Requested name. Copied into spec; clients can free the buffer passed
 *                      after this call.
 * \return              False if name is not a valid attribute of this surface type.
 */
bool mir_surface_plan_set_name(MirSurfacePlan* spec, char const* name);

/**
 * Set the requested width, in pixels
 *
 * \param [in] spec     Specification to mutate
 * \param [in] width    Requested width.
 * \return              False if width is invalid for a surface of this type
 * \note    The requested dimensions are a hint only. The server is not guaranteed to create a
 *          surface of any specific width or height.
 */
bool mir_surface_plan_set_width(MirSurfacePlan* spec, unsigned width);

/**
 * Set the requested height, in pixels
 *
 * \param [in] spec     Specification to mutate
 * \param [in] height   Requested height.
 * \return              False if height is invalid for a surface of this type
 * \note    The requested dimensions are a hint only. The server is not guaranteed to create a
 *          surface of any specific width or height.
 */
bool mir_surface_plan_set_height(MirSurfacePlan* spec, unsigned height);

/**
 * Set the requested pixel format.
 * \param [in] spec     Specification to mutate
 * \param [in] format   Requested pixel format
 * \return              False if format is not a valid pixel format for this surface type.
 */
bool mir_surface_plan_set_pixel_format(MirSurfacePlan* spec, MirPixelFormat format);

/**
 * Set the requested buffer usage.
 * \param [in] spec     Specification to mutate
 * \param [in] usage    Requested buffer usage
 * \return              False if the requested buffer usage is invalid for this surface.
 */
bool mir_surface_plan_set_buffer_usage(MirSurfacePlan* spec, MirBufferUsage usage);

/**
 * \param [in] spec         Specification to mutate
 * \param [in] output_id    ID of output to place surface on. From MirDisplayOutput.output_id
 * \return                  False if setting surface fullscreen is invalid for this surface.
 */
bool mir_surface_plan_set_fullscreen_on_output(MirSurfacePlan* spec, uint32_t output_id);

/**
 * Get a surface specification based on a surface's current state
 * \param [in] surf     Surface to generate specification from
 * \return              A surface specification that would construct a surface with
 *                      identical attributes to surf.
 * \note    Caller owns this MirSurfaceSpec. If the spec is not passed to mir_surface_realise
 *          use mir_surface_spec_release to free the associated resources.
 */
MirSurfacePlan* mir_surface_get_plan(MirSurface* surf);

/**
 * Get the requested name
 * \param [in] spec     Specification to query
 * \return              The name. This buffer is owned by the MirSurfaceSpec; do not free it.
 */
char const* mir_surface_plan_get_name(MirSurfacePlan* spec);

/**
 * Get the requested width
 * \param [in] spec     Specification to query
 * \return              The width, in pixels, or -1 if the width is unset
 */
int mir_surface_plan_get_width(MirSurfacePlan* spec);

/**
 * Get the requested height
 * \param [in] spec     Specification to query
 * \return              The height, in pixels, or -1 if the height is unset
 */
int mir_surface_plan_get_height(MirSurfacePlan* spec);

/**
 * Get the requested pixel format
 * \param [in] spec     Specification to query
 * \return              The pixel format
 */
MirPixelFormat mir_surface_plan_get_pixel_format(MirSurfacePlan* spec);

/**
 * Get the requested buffer usage
 * \param [in] spec     Specification to query
 * \return              The buffer usage
 */
MirBufferUsage mir_surface_plan_get_buffer_usage(MirSurfacePlan* spec);

/**
 * Release the resources held by a MirSurfaceSpec.
 *
 * \note mir_surface_realise/mir_surface_realise_sync consume the MirSurfaceSpec passed
 *       to them. It is incorrect to call mir_surface_spec_release on a MirSurfaceSpec
 *       passed to these functions.
 * \param [in] spec     Specification to release
 */
void mir_surface_plan_release(MirSurfacePlan* spec);

/**
 * Request a new Mir surface on the supplied connection with the supplied
 * parameters. The returned handle remains valid until the surface has been
 * released.
 *   \warning callback could be called from another thread. You must do any
 *            locking appropriate to protect your data accessed in the
 *            callback.
 *   \param [in] connection          The connection
 *   \param [in] surface_parameters  Request surface parameters
 *   \param [in] callback            Callback function to be invoked when
 *                                   request completes
 *   \param [in,out] context         User data passed to the callback function
 *   \return                         A handle that can be passed to
 *                                   mir_wait_for
 */
MirWaitHandle *mir_connection_create_surface(
    MirConnection *connection,
    MirSurfaceParameters const *surface_parameters,
    mir_surface_callback callback,
    void *context);

/**
 * Create a surface like in mir_connection_create_surface(), but also wait for
 * creation to complete and return the resulting surface.
 *   \param [in] connection  The connection
 *   \param [in] params      Parameters describing the desired surface
 *   \return                 The resulting surface
 */
MirSurface *mir_connection_create_surface_sync(
    MirConnection *connection,
    MirSurfaceParameters const *params);

/**
 * Set the event handler to be called when events arrive for a surface.
 *   \warning event_handler could be called from another thread. You must do
 *            any locking appropriate to protect your data accessed in the
 *            callback. There is also a chance that different events will be
 *            called back in different threads, for the same surface,
 *            simultaneously.
 *   \param [in] surface        The surface
 *   \param [in] event_handler  The event handler to call
 */
void mir_surface_set_event_handler(MirSurface *surface,
                                   MirEventDelegate const *event_handler);

/**
 * Get a window type that can be used for OpenGL ES 2.0 acceleration.
 *   \param [in] surface  The surface
 *   \return              An EGLNativeWindowType that the client can use
 */
MirEGLNativeWindowType mir_surface_get_egl_native_window(MirSurface *surface);

/**
 * Test for a valid surface
 *   \param [in] surface  The surface
 *   \return              True if the supplied surface is valid, or
 *                        false otherwise.
 */
bool mir_surface_is_valid(MirSurface *surface);

/**
 * Retrieve a text description of the error. The returned string is owned by
 * the library and remains valid until the surface or the associated
 * connection has been released.
 *   \param [in] surface  The surface
 *   \return              A text description of any error resulting in an
 *                        invalid surface, or the empty string "" if the
 *                        connection is valid.
 */
char const *mir_surface_get_error_message(MirSurface *surface);

/**
 * Get a surface's parameters.
 *   \pre                     The surface is valid
 *   \param [in] surface      The surface
 *   \param [out] parameters  Structure to be populated
 */
void mir_surface_get_parameters(MirSurface *surface, MirSurfaceParameters *parameters);

/**
 * Get the underlying platform type so the buffer obtained in "raw" representation
 * in mir_surface_get_current_buffer() can be understood
 *   \pre                     The surface is valid
 *   \param [in] surface      The surface
 *   \return                  One of mir_platform_type_android or mir_platform_type_gbm
 */
MirPlatformType mir_surface_get_platform_type(MirSurface *surface);

/**
 * Get a surface's buffer in "raw" representation.
 *   \pre                         The surface is valid
 *   \param [in] surface          The surface
 *   \param [out] buffer_package  Structure to be populated
 */
void mir_surface_get_current_buffer(MirSurface *surface, MirNativeBuffer **buffer_package);

/**
 * Get a surface's graphics_region, i.e., map the graphics buffer to main
 * memory.
 *   \pre                          The surface is valid
 *   \param [in] surface           The surface
 *   \param [out] graphics_region  Structure to be populated
 */
void mir_surface_get_graphics_region(
    MirSurface *surface,
    MirGraphicsRegion *graphics_region);

/**
 * Advance a surface's buffer. The returned handle remains valid until the next
 * call to mir_surface_swap_buffers, until the surface has been released or the
 * connection to the server has been released.
 *   \warning callback could be called from another thread. You must do any
 *            locking appropriate to protect your data accessed in the
 *            callback.
 *   \param [in] surface      The surface
 *   \param [in] callback     Callback function to be invoked when the request
 *                            completes
 *   \param [in,out] context  User data passed to the callback function
 *   \return                  A handle that can be passed to mir_wait_for
 */
MirWaitHandle *mir_surface_swap_buffers(
    MirSurface *surface,
    mir_surface_callback callback,
    void *context);

/**
 * Advance a surface's buffer as in mir_surface_swap_buffers(), but also wait
 * for the operation to complete.
 *   \param [in] surface  The surface whose buffer to advance
 */
void mir_surface_swap_buffers_sync(MirSurface *surface);

/**
 * Release the supplied surface and any associated buffer. The returned wait
 * handle remains valid until the connection to the server is released.
 *   \warning callback could be called from another thread. You must do any
 *            locking appropriate to protect your data accessed in the
 *            callback.
 *   \param [in] surface      The surface
 *   \param [in] callback     Callback function to be invoked when the request
 *                            completes
 *   \param [in,out] context  User data passed to the callback function
 *   \return                  A handle that can be passed to mir_wait_for
 */
MirWaitHandle *mir_surface_release(
    MirSurface *surface,
    mir_surface_callback callback,
    void *context);

/**
 * Release the specified surface like in mir_surface_release(), but also wait
 * for the operation to complete.
 *   \param [in] surface  The surface to be released
 */
void mir_surface_release_sync(MirSurface *surface);

/**
 * \deprecated Use mir_debug_surface_id()
 */
__attribute__((__deprecated__("Use mir_debug_surface_id()")))
int mir_surface_get_id(MirSurface *surface);

/**
 * Set the type (purpose) of a surface. This is not guaranteed to always work
 * with some shell types (e.g. phone/tablet UIs). As such, you may have to
 * wait on the function and check the result using mir_surface_get_type.
 *   \param [in] surface  The surface to operate on
 *   \param [in] type     The new type of the surface
 *   \return              A wait handle that can be passed to mir_wait_for
 */
MirWaitHandle* mir_surface_set_type(MirSurface *surface, MirSurfaceType type);

/**
 * Get the type (purpose) of a surface.
 *   \param [in] surface  The surface to query
 *   \return              The type of the surface
 */
MirSurfaceType mir_surface_get_type(MirSurface *surface);

/**
 * Change the state of a surface.
 *   \param [in] surface  The surface to operate on
 *   \param [in] state    The new state of the surface
 *   \return              A wait handle that can be passed to mir_wait_for
 */
MirWaitHandle* mir_surface_set_state(MirSurface *surface,
                                     MirSurfaceState state);

/**
 * Get the current state of a surface.
 *   \param [in] surface  The surface to query
 *   \return              The state of the surface
 */
MirSurfaceState mir_surface_get_state(MirSurface *surface);

/**
 * Set the swapinterval for mir_surface_swap_buffers. EGL users should use
 * eglSwapInterval directly.
 * At the time being, only swapinterval of 0 or 1 is supported.
 *   \param [in] surface  The surface to operate on
 *   \param [in] interval The number of vblank signals that
 *                        mir_surface_swap_buffers will wait for
 *   \return              A wait handle that can be passed to mir_wait_for,
 *                        or NULL if the interval could not be supported
 */
MirWaitHandle* mir_surface_set_swapinterval(MirSurface* surface, int interval);

/**
 * Query the swapinterval that the surface is operating with.
 * The default interval is 1.
 *   \param [in] surface  The surface to operate on
 *   \return              The swapinterval value that the client is operating with.
 *                        Returns -1 if surface is invalid.
 */
int mir_surface_get_swapinterval(MirSurface* surface);

/**
 * Query the DPI value of the surface (dots per inch). This will vary depending
 * on the physical display configuration and where the surface is within it.
 *   \return  The DPI of the surface, or zero if unknown.
 */
int mir_surface_get_dpi(MirSurface* surface);
    
/**
 * Query the focus state for a surface.
 *   \param [in] surface The surface to operate on
 *   \return             The focus state of said surface
 */
MirSurfaceFocusState mir_surface_get_focus(MirSurface *surface);

/**
 * Query the visibility state for a surface.
 *   \param [in] surface The surface to operate on
 *   \return             The visibility state of said surface
 */
MirSurfaceVisibility mir_surface_get_visibility(MirSurface *surface);

/**
 * Choose the cursor state for a surface: whether a cursor is shown, 
 * and which cursor if so.
 *    \param [in] surface    The surface to operate on
 *    \param [in] parameters The configuration parameters obtained
 *                           from mir_cursor* family of functions.
 *    \return                A wait handle that can be passed to mir_wait_for,
 *                           or NULL if parameters is invalid.
 *
 */
MirWaitHandle* mir_surface_configure_cursor(MirSurface *surface, MirCursorConfiguration const* parameters);

/**
 * Get the orientation of a surface.
 *   \param [in] surface  The surface to query
 *   \return              The orientation of the surface
 */
MirOrientation mir_surface_get_orientation(MirSurface *surface);

#ifdef __cplusplus
}
/**@}*/
#endif

#endif /* MIR_TOOLKIT_MIR_SURFACE_H_ */
