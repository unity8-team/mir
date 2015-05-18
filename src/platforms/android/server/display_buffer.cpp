/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "framebuffer_bundle.h"
#include "display_buffer.h"
#include "display_device.h"
#include "hwc_layerlist.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

mga::DisplayBuffer::DisplayBuffer(
    mga::DisplayName display_name,
    std::unique_ptr<LayerList> layer_list,
    std::shared_ptr<FramebufferBundle> const& fb_bundle,
    std::shared_ptr<DisplayDevice> const& display_device,
    std::shared_ptr<ANativeWindow> const& native_window,
    mga::GLContext const& shared_gl_context,
    mg::GLProgramFactory const& program_factory,
    MirOrientation orientation,
    mga::OverlayOptimization overlay_option)
    : display_name(display_name),
      layer_list(std::move(layer_list)),
      fb_bundle{fb_bundle},
      display_device{display_device},
      native_window{native_window},
      gl_context{shared_gl_context, fb_bundle, native_window},
      overlay_program{program_factory, gl_context, geom::Rectangle{{0,0},fb_bundle->fb_size()}},
      overlay_enabled{overlay_option == mga::OverlayOptimization::enabled},
      orientation_{orientation},
      power_mode_{mir_power_mode_on}
{
}

geom::Rectangle mga::DisplayBuffer::view_area() const
{
    auto const& size = fb_bundle->fb_size();
    int width = size.width.as_int();
    int height = size.height.as_int();

    if (orientation_ == mir_orientation_left || orientation_ == mir_orientation_right)
        std::swap(width, height);

    return {{0,0}, {width,height}};
}

void mga::DisplayBuffer::make_current()
{
    gl_context.make_current();
}

void mga::DisplayBuffer::release_current()
{
    gl_context.release_current();
}

bool mga::DisplayBuffer::post_renderables_if_optimizable(RenderableList const& renderlist)
{
    if (!overlay_enabled || !display_device->compatible_renderlist(renderlist))
        return false;

    layer_list->update_list(renderlist);

    bool needs_commit{false};
    for (auto& layer : *layer_list)
        needs_commit |= layer.needs_commit;

    return needs_commit;
}

void mga::DisplayBuffer::gl_swap_buffers()
{
    layer_list->update_list({});
}

MirOrientation mga::DisplayBuffer::orientation() const
{
    /*
     * android::DisplayBuffer is aways created with physical width/height
     * (not rotated). So we just need to pass through the desired rotation
     * and let the renderer do it.
     * If and when we choose to implement HWC rotation, this may change.
     */
    return orientation_;
}

void mga::DisplayBuffer::configure(MirPowerMode power_mode, MirOrientation orientation)
{
    power_mode_ = power_mode;
    if (power_mode_ != mir_power_mode_on)
        display_device->content_cleared();
    orientation_ = orientation;
}

mga::DisplayContents mga::DisplayBuffer::contents()
{
    return mga::DisplayContents{display_name, *layer_list, gl_context, overlay_program};
}

MirPowerMode mga::DisplayBuffer::power_mode() const
{
    return power_mode_;
}
