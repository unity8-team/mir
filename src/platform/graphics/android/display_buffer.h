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

#ifndef MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_

#include "configurable_display_buffer.h"
#include "mir/graphics/egl_resources.h"
#include "mir/graphics/gl_program_factory.h"
#include "android_display_configuration.h"
#include "gl_context.h"
#include "hwc_fallback_gl_renderer.h"
#include "overlay_optimization.h"
#include <system/window.h>

namespace mir
{
namespace graphics
{
namespace android
{

class DisplayDevice;
class FramebufferBundle;

class DisplayBuffer : public ConfigurableDisplayBuffer
{
public:
    DisplayBuffer(std::shared_ptr<FramebufferBundle> const& fb_bundle,
                  std::shared_ptr<DisplayDevice> const& display_device,
                  std::shared_ptr<ANativeWindow> const& native_window,
                  GLContext const& shared_gl_context,
                  GLProgramFactory const& program_factory,
                  OverlayOptimization overlay_option);

    geometry::Rectangle view_area() const;
    void make_current();
    void release_current();
    void post_update();
    bool post_renderables_if_optimizable(RenderableList const& renderlist);

    MirOrientation orientation() const override;
    bool uses_alpha() const override;

    DisplayConfigurationOutput configuration() const;
    void configure(DisplayConfigurationOutput const&);

private:
    void post();

    std::shared_ptr<FramebufferBundle> const fb_bundle;
    std::shared_ptr<DisplayDevice> const display_device;
    std::shared_ptr<ANativeWindow> const native_window;
    GLContext gl_context;
    HWCFallbackGLRenderer overlay_program;
    bool prepared;
    bool overlay_enabled;
    DisplayConfigurationOutput current_configuration;
    MirOrientation rotation;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_DISPLAY_BUFFER_H_ */
