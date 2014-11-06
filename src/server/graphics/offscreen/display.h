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
#include "display_configuration.h"
#include "mir/graphics/surfaceless_egl_context.h"

#include <mutex>
#include <vector>

#include <EGL/egl.h>

namespace mir
{
namespace graphics
{

class BasicPlatform;
class DisplayConfigurationPolicy;
class DisplayReport;

namespace offscreen
{
namespace detail
{

class EGLDisplayHandle
{
public:
    explicit EGLDisplayHandle(EGLNativeDisplayType native_type);
    EGLDisplayHandle(EGLDisplayHandle&&);
    ~EGLDisplayHandle() noexcept;

    void initialize();
    operator EGLDisplay() const { return egl_display; }

private:
    EGLDisplayHandle(EGLDisplayHandle const&) = delete;
    EGLDisplayHandle operator=(EGLDisplayHandle const&) = delete;

    EGLDisplay egl_display;
};

}

class Display : public graphics::Display
{
public:
    Display(std::shared_ptr<BasicPlatform> const& basic_platform,
            std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
            std::shared_ptr<DisplayReport> const& listener);
    ~Display() noexcept;

    void for_each_display_buffer(std::function<void(DisplayBuffer&)> const& f) override;

    std::unique_ptr<graphics::DisplayConfiguration> configuration() const override;
    void configure(graphics::DisplayConfiguration const& conf) override;

    void register_configuration_change_handler(
        EventHandlerRegister& handlers,
        DisplayConfigurationChangeHandler const& conf_change_handler) override;

    void register_pause_resume_handlers(
        EventHandlerRegister& handlers,
        DisplayPauseHandler const& pause_handler,
        DisplayResumeHandler const& resume_handler) override;

    void pause() override;
    void resume() override;

    std::shared_ptr<Cursor> create_hardware_cursor(std::shared_ptr<CursorImage> const& initial_image) override;
    std::unique_ptr<GLContext> create_gl_context() override;

private:
    std::shared_ptr<BasicPlatform> const basic_platform;
    detail::EGLDisplayHandle const egl_display;
    SurfacelessEGLContext const egl_context_shared;
    mutable std::mutex configuration_mutex;
    DisplayConfiguration current_display_configuration;
    std::vector<std::unique_ptr<DisplayBuffer>> display_buffers;
};

}
}
}

#endif /* MIR_GRAPHICS_OFFSCREEN_DISPLAY_H_ */
