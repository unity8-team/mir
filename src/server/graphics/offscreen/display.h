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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_OFFSCREEN_DISPLAY_H_
#define MIR_GRAPHICS_OFFSCREEN_DISPLAY_H_

#include "mir/graphics/display.h"
#include "mir/graphics/egl_resources.h"
#include "display_configuration.h"
#include "dummy_egl_surface.h"

#include <mutex>
#include <vector>

#include <EGL/egl.h>

namespace mir
{
namespace graphics
{

class OffscreenPlatform;
class DisplayConfigurationPolicy;
class DisplayReport;

namespace offscreen
{

class Display : public graphics::Display
{
public:
    Display(std::shared_ptr<OffscreenPlatform> const& offscreen_platform,
            std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
            std::shared_ptr<DisplayReport> const& listener);

    ~Display() noexcept;
    void for_each_display_buffer(std::function<void(DisplayBuffer&)> const& f);

    std::shared_ptr<graphics::DisplayConfiguration> configuration();
    void configure(graphics::DisplayConfiguration const& conf);

    void register_configuration_change_handler(
        EventHandlerRegister& handlers,
        DisplayConfigurationChangeHandler const& conf_change_handler);

    void register_pause_resume_handlers(
        EventHandlerRegister& handlers,
        DisplayPauseHandler const& pause_handler,
        DisplayResumeHandler const& resume_handler);

    void pause();
    void resume();

    std::weak_ptr<Cursor> the_cursor();
    std::unique_ptr<GLContext> create_gl_context();

private:
    std::shared_ptr<OffscreenPlatform> const offscreen_platform;
    EGLDisplay const egl_display;
    DummyEGLSurface const dummy_egl_surface;
    EGLContextStore const egl_context_shared;
    std::mutex configuration_mutex;
    DisplayConfiguration current_display_configuration;
    std::vector<std::unique_ptr<DisplayBuffer>> display_buffers;
};

}
}
}

#endif /* MIR_GRAPHICS_OFFSCREEN_DISPLAY_H_ */
