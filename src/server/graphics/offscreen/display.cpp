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

#include "display.h"
#include "display_buffer.h"
#include "mir/graphics/offscreen_platform.h"
#include "mir/graphics/display_configuration_policy.h"
#include "mir/graphics/gl_context.h"
#include "mir/geometry/size.h"

#include <boost/throw_exception.hpp>

namespace mg = mir::graphics;
namespace mgo = mg::offscreen;
namespace geom = mir::geometry;

namespace
{

EGLint const default_egl_context_attr[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

class OffscreenGLContext : public mg::GLContext
{
public:
    OffscreenGLContext(EGLDisplay egl_display, EGLContext egl_context_shared)
        : egl_display{egl_display},
          dummy_egl_surface{egl_display},
          egl_context{egl_display,
                      eglCreateContext(egl_display, dummy_egl_surface.config(),
                                       egl_context_shared, default_egl_context_attr)}
    {
    }

    ~OffscreenGLContext() noexcept
    {
        if (eglGetCurrentContext() == egl_context)
            release_current();
    }

    void make_current()
    {
        if (eglMakeCurrent(egl_display, dummy_egl_surface, dummy_egl_surface,
                           egl_context) == EGL_FALSE)
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error("could not activate dummy surface with eglMakeCurrent\n"));
        }
    }

    void release_current()
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

private:
    EGLDisplay const egl_display;
    mgo::DummyEGLSurface const dummy_egl_surface;
    mg::EGLContextStore const egl_context;
};

EGLDisplay create_and_initialize_display(mg::OffscreenPlatform& offscreen_platform)
{
    EGLint major, minor;

    auto egl_display = eglGetDisplay(offscreen_platform.egl_native_display());
    if (egl_display == EGL_NO_DISPLAY)
        BOOST_THROW_EXCEPTION(std::runtime_error("eglGetDisplay failed\n"));

    if (eglInitialize(egl_display, &major, &minor) == EGL_FALSE)
        BOOST_THROW_EXCEPTION(std::runtime_error("eglInitialize failure\n"));

    if ((major != 1) || (minor != 4))
        BOOST_THROW_EXCEPTION(std::runtime_error("must have EGL 1.4\n"));

    return egl_display;
}

EGLConfig choose_config(EGLDisplay egl_display)
{
    static EGLint const config_attr[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLConfig egl_config{0};
    int num_egl_configs{0};

    if (eglChooseConfig(egl_display, config_attr, &egl_config, 1, &num_egl_configs) == EGL_FALSE ||
        num_egl_configs != 1)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to choose ARGB EGL config"));
    }

    return egl_config;
}

}

mgo::Display::Display(
    std::shared_ptr<OffscreenPlatform> const& offscreen_platform,
    std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
    std::shared_ptr<DisplayReport> const&)
    : offscreen_platform{offscreen_platform},
      egl_display{create_and_initialize_display(*offscreen_platform)},
      dummy_egl_surface{egl_display},
      egl_context_shared{egl_display,
                         eglCreateContext(egl_display, dummy_egl_surface.config(),
                                          EGL_NO_CONTEXT,
                                          default_egl_context_attr)},
      current_display_configuration{geom::Size{1024,768}}
{
    /*
     * Make the shared context current. This needs to be done before we configure()
     * since mgo::DisplayBuffer creation needs a current GL context.
     */
    if (eglMakeCurrent(egl_display, dummy_egl_surface, dummy_egl_surface,
                       egl_context_shared) == EGL_FALSE)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("could not activate dummy surface with eglMakeCurrent\n"));
    }

    initial_conf_policy->apply_to(current_display_configuration);

    configure(current_display_configuration);
}

mgo::Display::~Display() noexcept
{
}

void mgo::Display::for_each_display_buffer(
    std::function<void(mg::DisplayBuffer&)> const& f)
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    for (auto& db_ptr : display_buffers)
        f(*db_ptr);
}

std::shared_ptr<mg::DisplayConfiguration> mgo::Display::configuration()
{
    std::lock_guard<std::mutex> lg{configuration_mutex};
    return std::make_shared<mgo::DisplayConfiguration>(current_display_configuration);
}

void mgo::Display::configure(mg::DisplayConfiguration const& conf)
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    display_buffers.clear();

    conf.for_each_output(
        [this] (DisplayConfigurationOutput const& output)
        {
            if (output.connected && output.preferred_mode_index < output.modes.size())
            {
                geom::Rectangle const area{
                    output.top_left, output.modes[output.current_mode_index].size};
                auto raw_db = new mgo::DisplayBuffer{egl_display,
                                                     egl_context_shared, area};

                display_buffers.push_back(std::unique_ptr<mg::DisplayBuffer>(raw_db));
            }
        });
}

void mgo::Display::register_configuration_change_handler(
        EventHandlerRegister&,
        DisplayConfigurationChangeHandler const&)
{
}

void mgo::Display::register_pause_resume_handlers(
        EventHandlerRegister&,
        DisplayPauseHandler const&,
        DisplayResumeHandler const&)
{
}

void mgo::Display::pause()
{
}

void mgo::Display::resume()
{
}

std::weak_ptr<mg::Cursor> mgo::Display::the_cursor()
{
    return {};
}

std::unique_ptr<mg::GLContext> mgo::Display::create_gl_context()
{
    return std::unique_ptr<GLContext>{
        new OffscreenGLContext{egl_display, egl_context_shared}};
}
