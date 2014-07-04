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

#ifndef MIR_TOOLKIT_MIR_CONNECTION_H_
#define MIR_TOOLKIT_MIR_CONNECTION_H_

#include <mir_toolkit/client_types.h>
#include <mir_toolkit/common.h>

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

/**
 * Request a connection to the Mir server. The supplied callback is called when
 * the connection is established, or fails. The returned wait handle remains
 * valid until the connection has been released.
 *   \warning callback could be called from another thread. You must do any
 *            locking appropriate to protect your data accessed in the
 *            callback.
 *   \param [in] server       File path of the server socket to connect to, or
 *                            NULL to choose the default server (can be set by
 *                            the $MIR_SOCKET environment variable)
 *   \param [in] app_name     A name referring to the application
 *   \param [in] callback     Callback function to be invoked when request
 *                            completes
 *   \param [in,out] context  User data passed to the callback function
 *   \return                  A handle that can be passed to mir_wait_for
 */
MirWaitHandle *mir_connect(
    char const *server,
    char const *app_name,
    mir_connected_callback callback,
    void *context);

/**
 * Perform a mir_connect() but also wait for and return the result.
 *   \param [in] server    File path of the server socket to connect to, or
 *                         NULL to choose the default server
 *   \param [in] app_name  A name referring to the application
 *   \return               The resulting MirConnection
 */
MirConnection *mir_connect_sync(char const *server, char const *app_name);

/**
 * Test for a valid connection
 * \param [in] connection  The connection
 * \return                 True if the supplied connection is valid, or
 *                         false otherwise.
 */
MirBool mir_connection_is_valid(MirConnection *connection);

/**
 * Retrieve a text description of the last error. The returned string is owned
 * by the library and remains valid until the connection has been released.
 *   \param [in] connection  The connection
 *   \return                 A text description of any error resulting in an
 *                           invalid connection, or the empty string "" if the
 *                           connection is valid.
 */
char const *mir_connection_get_error_message(MirConnection *connection);

/**
 * Release a connection to the Mir server
 *   \param [in] connection  The connection
 */
void mir_connection_release(MirConnection *connection);

/**
 * Query platform-specific data and/or file descriptors that are required to
 * initialize GL/EGL features.
 *   \param [in]  connection        The connection
 *   \param [out] platform_package  Structure to be populated
 */
void mir_connection_get_platform(MirConnection *connection, MirPlatformPackage *platform_package);

/**
 * Register a callback to be called when a Lifecycle state change occurs.
 *   \param [in] connection     The connection
 *   \param [in] callback       The function to be called when the state change occurs
 *   \param [in,out] context    User data passed to the callback function
 */
void mir_connection_set_lifecycle_event_callback(MirConnection* connection,
    mir_lifecycle_event_callback callback, void* context);

/**
 * \deprecated Use mir_connection_create_display_config
 */
__attribute__((__deprecated__("Use mir_connection_create_display_config()")))
void mir_connection_get_display_info(MirConnection *connection, MirDisplayInfo *display_info);

/**
 * Query the display
 *   \warning return value must be destroyed via mir_display_config_destroy()
 *   \warning may return null if connection is invalid
 *   \param [in]  connection        The connection
 *   \return                        structure that describes the display configuration
 */
MirDisplayConfiguration* mir_connection_create_display_config(MirConnection *connection);

/**
 * Register a callback to be called when the hardware display configuration changes
 *
 * Once a change has occurred, you can use mir_connection_create_display_config to see
 * the new configuration.
 *
 *   \param [in] connection  The connection
 *   \param [in] callback     The function to be called when a display change occurs
 *   \param [in,out] context  User data passed to the callback function
 */
void mir_connection_set_display_config_change_callback(
    MirConnection* connection,
    mir_display_config_callback callback, void* context);

/**
 * Destroy the DisplayConfiguration resource acquired from mir_connection_create_display_config
 *   \param [in] display_configuration  The display_configuration information resource to be destroyed
 */
void mir_display_config_destroy(MirDisplayConfiguration* display_configuration);

/**
 * Apply the display configuration
 *
 * The display configuration is applied to this connection only (per-connection
 * configuration) and is invalidated when a hardware change occurs. Clients should
 * register a callback with mir_connection_set_display_config_change_callback()
 * to get notified about hardware changes, so that the can apply a new configuration.
 *
 *   \warning This request may be denied. Check that the request succeeded with mir_connection_get_error_message.
 *   \param [in] connection             The connection
 *   \param [in] display_configuration  The display_configuration to apply
 *   \return                            A handle that can be passed to mir_wait_for
 */
MirWaitHandle* mir_connection_apply_display_config(MirConnection *connection, MirDisplayConfiguration* display_configuration);

/**
 * Get a display type that can be used for OpenGL ES 2.0 acceleration.
 *   \param [in] connection  The connection
 *   \return                 An EGLNativeDisplayType that the client can use
 */
MirEGLNativeDisplayType mir_connection_get_egl_native_display(MirConnection *connection);

/**
 * Get the list of possible formats that a surface can be created with.
 *   \param [in] connection         The connection
 *   \param [out] formats           List of valid formats to create surfaces with
 *   \param [in]  formats_size      size of formats list
 *   \param [out] num_valid_formats number of valid formats returned in formats
 */
void mir_connection_get_available_surface_formats(
    MirConnection* connection, MirPixelFormat* formats,
    unsigned const int format_size, unsigned int *num_valid_formats);

#ifdef __cplusplus
}
/**@}*/
#endif

#endif /* MIR_TOOLKIT_MIR_CONNECTION_H_ */
