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
#include "mir/display_changer.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/input/scene.h"

#include <mutex>
#include <sstream>

namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

namespace
{
void update_rectangles(mg::DisplayConfiguration const& conf, geom::Rectangles& rectangles)
{
    conf.for_each_output(
        [&rectangles](mg::DisplayConfigurationOutput const& output)
        {
            if (output.power_mode == mir_power_mode_on &&
                output.current_mode_index < output.modes.size())
                rectangles.add({output.top_left, output.modes[output.current_mode_index].size});
        });
}
geom::Point transform(float scale,
                      geom::Displacement const& vector,
                      geom::Displacement const& hotspot,
                      geom::Size const& buffer_size,
                      MirOrientation orientation)
{
    auto center = geom::Displacement{buffer_size.width.as_int(), buffer_size.height.as_int()}*0.5f;
    auto scaled_center = scale*center;
    auto scaled_hotspot = scale*hotspot;

    switch(orientation)
    {
    case mir_orientation_right:
        scaled_hotspot = geom::Displacement{-scaled_hotspot.dy.as_int(), scaled_hotspot.dx.as_int()};
        scaled_center = geom::Displacement{-scaled_center.dy.as_int(), scaled_center.dx.as_int()};
        break;
    case mir_orientation_left:
        scaled_hotspot = geom::Displacement{scaled_hotspot.dy.as_int(), -scaled_hotspot.dx.as_int()};
        scaled_center = geom::Displacement{scaled_center.dy.as_int(), -scaled_center.dx.as_int()};
        break;
    case mir_orientation_inverted:
        scaled_center = -1.0f*scaled_center;
        scaled_hotspot = -1.0f*scaled_hotspot;
        break;
    default:
        break;
    }

    auto position = vector + scaled_center - scaled_hotspot - center;
    return {position.dx.as_int(), position.dy.as_int()};
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


    float get_scale()
    {
        return this->scale;
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

    void move_to(geom::Point new_position)
    {
        std::lock_guard<std::mutex> lock{position_mutex};
        position = new_position;
    }

private:
    std::shared_ptr<mg::Buffer> const buffer_;
    mutable std::mutex position_mutex;
    geom::Point position;
    float scale{1.0f};
    MirOrientation orientation{mir_orientation_normal};
};

mg::SoftwareCursor::SoftwareCursor(
    mg::DisplayConfiguration const& intial_configuration,
    std::shared_ptr<mir::DisplayChanger> const& display_changer,
    std::shared_ptr<mg::GraphicBufferAllocator> const& allocator,
    std::shared_ptr<mi::Scene> const& scene)
    : allocator{allocator},
      scene{scene},
      format{get_8888_format(allocator->supported_pixel_formats())},
      visible(false),
      hotspot{0,0}
{
    update_rectangles(intial_configuration, bounding_rectangle);
    display_changer->register_change_callback(
        [this](mg::DisplayConfiguration const& conf)
        {
            std::unique_lock<std::mutex> lock(guard);
            update_rectangles(conf, bounding_rectangle);
        });
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
    MirOrientation overridden = overrides.get_orientation(display_id, mir_orientation_normal);
    geom::Rectangle area = bounding_rectangle.bounding_rectangle();

    auto displacement = transform(cursor->get_scale(),
                                  position - area.top_left,
                                  hotspot,
                                  cursor->buffer()->size(),
                                  overridden);

    cursor->move_to(displacement);
    cursor->set_orientation(overridden);
}

void mg::SoftwareCursor::show(CursorImage const& cursor_image)
{
    std::shared_ptr<detail::CursorRenderable> new_renderable;
    std::shared_ptr<detail::CursorRenderable> old_renderable;

    // Do a lock dance to make this function threadsafe,
    // while avoiding calling scene methods under lock
    {
        geom::Point position{0,0};
        std::lock_guard<std::mutex> lg{guard};
        if (renderable)
            position = renderable->screen_position().top_left;
        new_renderable = create_renderable_for(cursor_image, position);
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
mg::SoftwareCursor::create_renderable_for(CursorImage const& cursor_image, geom::Point position)
{
    auto new_renderable = std::make_shared<detail::CursorRenderable>(
        allocator->alloc_buffer({cursor_image.size(), format, mg::BufferUsage::software}),
        position + hotspot - cursor_image.hotspot());

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
    scene->emit_scene_changed();
}
