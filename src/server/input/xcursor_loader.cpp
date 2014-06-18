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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "xcursor_loader.h"

#include "mir/graphics/cursor_image.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>

#include <string.h>

// Unforunately this can not be compiled as C++...so we can not namespace
// these symbols. In order to differentiate from internal symbols
//  we refer to them via their _ prefixed version, i.e. _XcursorImage
extern "C"
{
#include <xcursor.h>
}

namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

namespace
{
// TODO: Rename or standardize on _
struct XCursorImage : public mg::CursorImage
{
    XCursorImage(_XcursorImage *image)
        : image(image)
    {
        if (image->size != mg::default_cursor_size.width.as_uint32_t())
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error("Somehow we got a cursor not of the default size (currently only 64x64 supported)"));
        }
    }

    ~XCursorImage()
    {
    }

    void const* as_argb_8888() const
    {
        return image->pixels;
    }
    geom::Size size() const
    {
        return mg::default_cursor_size;
    }

    _XcursorImage *image;
};
}

mi::XCursorLoader::XCursorLoader()
{
    load_cursor_theme("default");
}

// Each XcursorImages represents images for the different sizes of a given symbolic cursor.
void mi::XCursorLoader::load_appropriately_sized_image(_XcursorImages *images)
{
    // TODO: We would rather take this lock in load_cursor_theme but the Xcursor lib style 
    // makes it difficult to use our standard 'pass the lg around to _locked members' pattern
    std::lock_guard<std::mutex> lg(guard);

    // We have to save all the images as XCursor expects us to free them.
    // TODO: Should probably share ownership with the image...
    resources.push_back(ImagesUPtr(images, [](_XcursorImages *images)
        {
            XcursorImagesDestroy(images);
        }));

    _XcursorImage *image_of_correct_size = nullptr;
    for (int i = 0; i < images->nimage; i++)
    {
        _XcursorImage *candidate = images->images[i];
        if (candidate->width == mg::default_cursor_size.width.as_uint32_t() &&
            candidate->height == mg::default_cursor_size.height.as_uint32_t())
        {
            image_of_correct_size = candidate;
            break;
        }
    }
    if (!image_of_correct_size)
        return;
    loaded_images[std::string(images->name)] = std::make_shared<XCursorImage>(image_of_correct_size);
}

void mi::XCursorLoader::load_cursor_theme(std::string const& theme_name)
{
 
    xcursor_load_theme(theme_name.c_str(), mg::default_cursor_size.width.as_uint32_t(),
        [](XcursorImages* images, void *this_ptr)  -> void
        {
            // Can't use lambda capture as this lambda is thunked to a C function ptr
            auto p = static_cast<mi::XCursorLoader*>(this_ptr);
            p->load_appropriately_sized_image(images);
            
        }, this);
}

std::shared_ptr<mg::CursorImage> mi::XCursorLoader::image(std::string const& cursor_name,
    geom::Size const& size)
{
    // 'arrow' is the default cursor in XCursor themes.
    if (cursor_name == "default")
        return image("arrow", size);

    if (size != mg::default_cursor_size)
        BOOST_THROW_EXCEPTION(
            std::logic_error("Only the default cursor size is currently supported (mg::default_cursor_size)"));

    auto it = loaded_images.find(cursor_name);
    if (it != loaded_images.end())
        return it->second;

    // Fall back
    it = loaded_images.find("arrow");
    if (it != loaded_images.end())
        return it->second;
    
    return nullptr;
}
