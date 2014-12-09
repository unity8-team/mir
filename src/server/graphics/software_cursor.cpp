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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "software_cursor.h"
#include "mir/graphics/cursor_image.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/buffer_writer.h"
#include "mir/graphics/pixel_format_utils.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/input/scene.h"

#include <mutex>
#include <atomic>

namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

namespace
{

MirPixelFormat get_8888_format(std::vector<MirPixelFormat> const& formats)
{
    for (auto format : formats)
    {
        if (mg::red_channel_depth(format) == 8 &&
            mg::green_channel_depth(format) == 8 &&
            mg::blue_channel_depth(format) == 8 &&
            mg::alpha_channel_depth(format) == 8)
        {
            return format;
        }
    }

    return mir_pixel_format_invalid;
}

}

class mg::detail::CursorRenderable : public mg::Renderable
{
public:
    CursorRenderable(std::shared_ptr<mg::Buffer> const& buffer,
                     geom::Point const& position)
        : buffer_{buffer},
          position{position}
    {
    }

    mg::Renderable::ID id() const override
    {
        return this;
    }

    std::shared_ptr<mg::Buffer> buffer() const override
    {
        std::lock_guard<std::mutex> lock{buffer_mutex};
        return buffer_;
    }

    geom::Rectangle screen_position() const override
    {
        return {position, buffer_->size()};
    }

    float alpha() const override
    {
        return 1.0;
    }

    glm::mat4 transformation() const override
    {
        return glm::mat4();
    }

    bool visible() const override
    {
        return true;
    }

    bool shaped() const override
    {
        return true;
    }

    int buffers_ready_for_compositor() const override
    {
        return 1;
    }

    void move_to(geom::Point new_position)
    {
        position = new_position;
    }

    std::unique_lock<std::mutex> lock_buffer()
    {
        return std::unique_lock<std::mutex>{buffer_mutex};
    }

private:
    std::shared_ptr<mg::Buffer> const buffer_;
    mutable std::mutex buffer_mutex;
    std::atomic<geom::Point> position;
};

mg::SoftwareCursor::SoftwareCursor(
    std::shared_ptr<mg::GraphicBufferAllocator> const& allocator,
    std::shared_ptr<mg::BufferWriter> const& buffer_writer,
    std::shared_ptr<mi::Scene> const& scene)
    : allocator{allocator},
      buffer_writer{buffer_writer},
      scene{scene},
      format{get_8888_format(allocator->supported_pixel_formats())},
      hotspot{0,0}
{
}

mg::SoftwareCursor::~SoftwareCursor()
{
    hide();
}

void mg::SoftwareCursor::show(CursorImage const& cursor_image)
{
    std::shared_ptr<detail::CursorRenderable> new_renderable;
    std::shared_ptr<detail::CursorRenderable> old_renderable;

    // Do a lock dance to make this function threadsafe,
    // while avoiding calling scene methods under lock
    {
        std::lock_guard<std::mutex> lg{guard};
        new_renderable = create_renderable_for(cursor_image);
    }

    // Add new renderable first, then remove old one to avoid visual glitches
    scene->add_input_visualization(new_renderable);

    // The second part of the lock dance
    {
        std::lock_guard<std::mutex> lg{guard};
        old_renderable = renderable;
        renderable = new_renderable;
        hotspot = cursor_image.hotspot();
    }

    if (old_renderable)
        scene->remove_input_visualization(old_renderable);
}

auto mg::SoftwareCursor::create_renderable_for(CursorImage const& cursor_image)
    -> std::shared_ptr<detail::CursorRenderable>
{
    std::shared_ptr<detail::CursorRenderable> new_renderable;

    // Reuse buffer if possible, to minimize buffer reallocations
    if (renderable && renderable->buffer()->size() == cursor_image.size())
    {
        new_renderable = std::make_shared<detail::CursorRenderable>(
            renderable->buffer(),
            renderable->screen_position().top_left + hotspot -
                cursor_image.hotspot());
    }
    else
    {
        auto const buffer = allocator->alloc_buffer(
            {cursor_image.size(), format, mg::BufferUsage::software});
        new_renderable = std::make_shared<detail::CursorRenderable>(
            buffer,
            geom::Point{0,0} - cursor_image.hotspot());
    }

    unsigned int const pixels_size =
        cursor_image.size().width.as_uint32_t() *
        cursor_image.size().height.as_uint32_t() *
        MIR_BYTES_PER_PIXEL(format);

    std::unique_lock<std::mutex> lock;

    // If we are reusing the buffer we need to lock it to ensure it's not
    // used while being updated
    if (renderable && renderable->buffer() == new_renderable->buffer())
        lock = renderable->lock_buffer();

    // TODO: The buffer pixel format may not be argb_8888, leading to
    // incorrect cursor colors. We need to transform the data to match
    // the buffer pixel format.
    buffer_writer->write(
        *new_renderable->buffer(),
        static_cast<unsigned char const*>(cursor_image.as_argb_8888()),
        pixels_size);

    return new_renderable;
}

void mg::SoftwareCursor::hide()
{
    decltype(renderable) tmp_renderable;

    {
        std::lock_guard<std::mutex> lg{guard};
        tmp_renderable = renderable;
        renderable = nullptr;
    }

    if (tmp_renderable)
        scene->remove_input_visualization(tmp_renderable);
}

void mg::SoftwareCursor::move_to(geometry::Point position)
{
    {
        std::lock_guard<std::mutex> lg{guard};

        if (!renderable)
            return;

        renderable->move_to(position - hotspot);
    }

    scene->emit_scene_changed();
}
