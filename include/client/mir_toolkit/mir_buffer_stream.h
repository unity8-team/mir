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
 */

#ifndef MIR_TOOLKIT_MIR_BUFFER_STREAM_H_
#define MIR_TOOLKIT_MIR_BUFFER_STREAM_H_

#include <mir_toolkit/mir_native_buffer.h>
#include <mir_toolkit/client_types.h>

#ifdef __cplusplus
/**
 * \addtogroup mir_toolkit
 * @{
 */
extern "C" {
#endif

MirWaitHandle* mir_buffer_stream_create(MirConnection *connection,
    int width, int height,
    MirPixelFormat format,
    MirBufferUsage buffer_usage,
    mir_buffer_stream_callback callback, 
    void* context);

MirBufferStream* mir_buffer_stream_create_sync(MirConnection *connection,
    int width, int height,
    MirPixelFormat format,
    MirBufferUsage buffer_usage);

/**
 * Release the supplied stream and any associated buffer. The returned wait
 * handle remains valid until the connection to the server is released.
 *   \warning callback could be called from another thread. You must do any
 *            locking appropriate to protect your data accessed in the
 *            callback.
 *   \param [in] stream      The stream
 *   \param [in] callback     Callback function to be invoked when the request
 *                            completes
 *   \param [in,out] context  User data passed to the callback function
 *   \return                  A handle that can be passed to mir_wait_for
 */
MirWaitHandle *mir_buffer_stream_release(
    MirBufferStream * buffer_stream,
    mir_buffer_stream_callback callback,
    void *context);

/**
 * Release the specified buffer stream like in mir_buffer_stream_release(), but also wait
 * for the operation to complete.
 *   \param [in] buffer stream  The buffer stream to be released
 */
void mir_buffer_stream_release_sync(MirBufferStream *buffer_stream);

#ifdef __cplusplus
}
/**@}*/
#endif

#endif // MIR_TOOLKIT_MIR_BUFFER_STREAM_H_
