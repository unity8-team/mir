/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_MESA_DISPLAY_GROUP_H_
#define MIR_GRAPHICS_MESA_DISPLAY_GROUP_H_

#include "mir/graphics/display.h"
#include "display_buffer.h"

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

class DisplayGroup : public graphics::DisplayGroup
{
public:
    DisplayGroup(
        std::shared_ptr<Platform> const& platform,
        std::shared_ptr<DisplayReport> const& listener,
        std::vector<std::shared_ptr<KMSOutput>> const& outputs,
        GBMSurfaceUPtr surface_gbm,
        geometry::Rectangle const& area,
        MirOrientation rot,
        GLConfig const& gl_config,
        EGLContext shared_context);
    ~DisplayGroup();

    void for_each_display_buffer(
        std::function<void(graphics::DisplayBuffer&)> const& f) override;
    void post() override;

    void schedule_set_crtc();
    void wait_for_page_flip();
    void set_orientation(MirOrientation const rot, geometry::Rectangle const& a);

private:
    bool schedule_page_flip(BufferObject* bufobj);

    DisplayBuffer db;
    std::shared_ptr<Platform> const platform;
    std::shared_ptr<DisplayReport> const listener;

    std::atomic<bool> needs_set_crtc;
    bool page_flips_pending;
    std::vector<std::shared_ptr<KMSOutput>> outputs;

    BufferObject* last_flipped_bufobj;
    BufferObject* scheduled_bufobj;
    std::shared_ptr<graphics::Buffer> last_flipped_bypass_buf;
};

}
}
}

#endif /* MIR_GRAPHICS_MESA_DISPLAY_GROUP_H_ */
