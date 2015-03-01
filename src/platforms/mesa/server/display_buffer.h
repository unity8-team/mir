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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_MESA_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_MESA_DISPLAY_BUFFER_H_

#include "mir/graphics/display_buffer.h"
#include "display_helpers.h"

#include <vector>
#include <memory>
#include <atomic>

namespace mir
{
namespace graphics
{

class DisplayReport;
class GLConfig;

namespace mesa
{

class Platform;
class BufferObject;
class KMSOutput;

class DisplayBuffer : public graphics::DisplayBuffer
{
public:
    DisplayBuffer(std::shared_ptr<Platform> const& platform,
                  std::shared_ptr<DisplayReport> const& listener,
                  std::vector<std::shared_ptr<KMSOutput>> const& outputs,
                  GBMSurfaceUPtr surface_gbm,
                  geometry::Rectangle const& area,
                  MirOrientation rot,
                  GLConfig const& gl_config,
                  EGLContext shared_context);
    ~DisplayBuffer();

    geometry::Rectangle view_area() const override;
    void make_current() override;
    void release_current() override;
    void gl_swap_buffers() override;
    void flip() override;
    bool post_renderables_if_optimizable(RenderableList const& renderlist) override;

    MirOrientation orientation() const override;
    void set_orientation(MirOrientation const rot, geometry::Rectangle const& a);
    bool uses_alpha() const override;
    void schedule_set_crtc();
    void wait_for_page_flip();

private:
    bool flip(std::shared_ptr<graphics::Buffer> bypass_buf);

    BufferObject* get_front_buffer_object();
    BufferObject* get_buffer_object(struct gbm_bo *bo);
    bool schedule_page_flip(BufferObject* bufobj);

    BufferObject* last_flipped_bufobj;
    BufferObject* scheduled_bufobj;
    std::shared_ptr<graphics::Buffer> last_flipped_bypass_buf;
    std::shared_ptr<Platform> const platform;
    std::shared_ptr<DisplayReport> const listener;
    /* DRM helper from mgm::Platform */
    helpers::DRMHelper& drm;
    std::vector<std::shared_ptr<KMSOutput>> outputs;
    GBMSurfaceUPtr surface_gbm;
    helpers::EGLHelper egl;
    geometry::Rectangle area;
    uint32_t fb_width, fb_height;
    MirOrientation rotation;
    std::atomic<bool> needs_set_crtc;
    bool page_flips_pending;
};

}
}
}

#endif /* MIR_GRAPHICS_MESA_DISPLAY_BUFFER_H_ */
