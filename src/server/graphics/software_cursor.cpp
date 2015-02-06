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
#include "mir/graphics/pixel_format_utils.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/input/scene.h"

#include <mutex>
#include <sstream>
#include <iostream>

namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

namespace
{
geom::Displacement transform(geom::Rectangle const& rect, geom::Displacement const& vector, MirOrientation orientation)
{
    switch(orientation)
    {
    case mir_orientation_left:
        return {vector.dy.as_int(), rect.size.width.as_int() -vector.dx.as_int()};
    case mir_orientation_inverted:
        return {rect.size.width.as_int() -vector.dx.as_int(), rect.size.height.as_int() - vector.dy.as_int()};
    case mir_orientation_right:
        return {rect.size.height.as_int() -vector.dy.as_int(), vector.dx.as_int()};
    default:
    case mir_orientation_normal:
        return vector;
    }
}

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
        auto const* env = getenv("GRID_UNIT_PX");
        if (env)
        {
            std::stringstream gu_parse(std::string{env});
            float value;
            if (gu_parse >> value)
                scale = value/8.0f; // guess to transform gu into a scale that turns 32x32 icon into a readable size
        }
    }

    mg::Renderable::ID id() const override
    {
        return this;
    }

    std::shared_ptr<mg::Buffer> buffer() const override
    {
        return buffer_;
    }

    geom::Rectangle screen_position() const override
    {
        std::lock_guard<std::mutex> lock{position_mutex};
        return {position, buffer_->size()};
    }

    float alpha() const override
    {
        return 1.0;
    }

    void set_scale(float scale)
    {
        this->scale = scale;
    }

    void set_orientation(MirOrientation orientation)
    {
        this->orientation = orientation;
    }

    glm::mat4 transformation() const override
    {
        switch(orientation)
        {
        case  mir_orientation_left:
            return glm::mat4( 0, -scale, 0, 0,
                             scale, 0, 0, 0,
                              0, 0, 1, 0,
                              0, 0, 0, 1);
        case  mir_orientation_inverted:
            return glm::mat4(-scale, 0, 0, 0,
                             0, -scale, 0, 0,
                             0, 0, 1, 0,
                             0, 0, 0, 1);
        case  mir_orientation_right:
            return glm::mat4( 0,scale, 0, 0,
                              -scale, 0, 0, 0,
                              0, 0, 1, 0,
                              0, 0, 0, 1);
        default:
        case  mir_orientation_normal:
            return glm::mat4(scale, 0, 0, 0,
                             0, scale, 0, 0,
                             0, 0, 1, 0,
                             0, 0, 0, 1);
        }
    }

    bool shaped() const override
    {
        return true;
    }

    int buffers_ready_for_compositor() const override
    {
        return 1;
    }

    void move_to(geom::Displacement new_position)
    {
        std::lock_guard<std::mutex> lock{position_mutex};
        position = geom::Point{new_position.dx.as_int(), new_position.dy.as_int()};
    }

private:
    std::shared_ptr<mg::Buffer> const buffer_;
    mutable std::mutex position_mutex;
    geom::Point position;
    float scale{1.0f};
    MirOrientation orientation{mir_orientation_normal};
};

mg::SoftwareCursor::SoftwareCursor(
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mg::GraphicBufferAllocator> const& allocator,
    std::shared_ptr<mi::Scene> const& scene)
    : allocator{allocator},
      scene{scene},
      format{get_8888_format(allocator->supported_pixel_formats())},
      visible(false),
      hotspot{0,0},
      display{display}
{
}

mg::SoftwareCursor::~SoftwareCursor()
{
    hide();
}

void mg::SoftwareCursor::show()
{
    bool needs_scene_change = false;
    {
        std::lock_guard<std::mutex> lg{guard};
        if (!visible)
            visible = needs_scene_change = true;
    }
    if (needs_scene_change && renderable)
    {
        update_visualization(renderable);
        scene->add_input_visualization(renderable);
    }
}

void mg::SoftwareCursor::update_visualization(std::shared_ptr<detail::CursorRenderable> cursor)
{
    if (!cursor)
        return;
    uint32_t display_id = 0;
    display->for_each_display_buffer(
        [cursor,&display_id,this](DisplayBuffer const& buffer)
        {
            MirOrientation overridden = overrides.get_orientation(display_id, buffer.orientation());

            auto extents = overrides.transform_rectangle(display_id, buffer.view_area(), buffer.orientation());

            if (!extents.contains(position))
                return;

            // this is actually wrong
            auto displacement = transform(extents, position - geometry::Point{0,0}, overridden);

            cursor->move_to(displacement - hotspot);
            cursor->set_orientation(overridden);

            ++display_id;
        });
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
        visible = true;
    }

    // Add the new renderable first, then remove the old one to avoid
    // visual glitches
    scene->add_input_visualization(new_renderable);

    // The second part of the lock dance
    {
        std::lock_guard<std::mutex> lg{guard};
        old_renderable = renderable;
        renderable = new_renderable;
        hotspot = cursor_image.hotspot();
    }
    update_visualization(new_renderable);

    if (old_renderable)
        scene->remove_input_visualization(old_renderable);
}

std::shared_ptr<mg::detail::CursorRenderable>
mg::SoftwareCursor::create_renderable_for(CursorImage const& cursor_image)
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

    size_t const pixels_size =
        cursor_image.size().width.as_uint32_t() *
        cursor_image.size().height.as_uint32_t() *
        MIR_BYTES_PER_PIXEL(format);

    // TODO: The buffer pixel format may not be argb_8888, leading to
    // incorrect cursor colors. We need to transform the data to match
    // the buffer pixel format.
    new_renderable->buffer()->write(
        static_cast<unsigned char const*>(cursor_image.as_argb_8888()),
        pixels_size);

    return new_renderable;
}

void mg::SoftwareCursor::hide()
{
    bool needs_scene_change = false;
    {
        std::lock_guard<std::mutex> lg{guard};
        if (visible)
        {
            visible = false;
            needs_scene_change = true;
        }
    }
    if (needs_scene_change && renderable)
        scene->remove_input_visualization(renderable);
}

void mg::SoftwareCursor::move_to(geometry::Point position)
{
    {
        std::lock_guard<std::mutex> lg{guard};

        this->position = position;

        if (!renderable)
            return;

        update_visualization(renderable);
    }

    scene->emit_scene_changed();
}

void mg::SoftwareCursor::override_orientation(uint32_t screen, MirOrientation orientation)
{
    // there is no cache invalidation in this hack
    overrides.add_override(screen, orientation);
}
