/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored By: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_SHELL_BUFFER_PIXELS_H_
#define MIR_SHELL_BUFFER_PIXELS_H_

#include "mir/geometry/size.h"
#include "mir/geometry/dimensions.h"

namespace mir
{
namespace compositor
{
class Buffer;
}
namespace shell
{

struct BufferPixelsData
{
    geometry::Size size;
    geometry::Stride stride;
    void* pixels;
};

/**
 * Interface for extracting the pixels from a compositor::Buffer.
 */
class BufferPixels
{
public:
    virtual ~BufferPixels() = default;

    /**
     * Extracts the pixels from a buffer.
     *
     * \param [in] buffer the buffer to get the pixels of
     */
    virtual void extract_from(compositor::Buffer& buffer) = 0;

    /**
     * The extracted pixels in 0xAARRGGBB format.
     *
     * The pixel data is owned by the BufferPixels object and is only valid
     * until the next call to extract_from().
     *
     * This method may involve transformation of the extracted data.
     */
    virtual BufferPixelsData as_argb_8888() = 0;

protected:
    BufferPixels() = default;
    BufferPixels(BufferPixels const&) = delete;
    BufferPixels& operator=(BufferPixels const&) = delete;
};

}
}

#endif /* MIR_SHELL_BUFFER_PIXELS_H_ */
