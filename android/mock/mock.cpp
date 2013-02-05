/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#include <ubuntu/application/ui/init.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/session_credentials.h>
#include <ubuntu/application/ui/setup.h>
#include <ubuntu/application/ui/surface.h>
#include <ubuntu/application/ui/surface_factory.h>
#include <ubuntu/application/ui/surface_properties.h>

#include <ubuntu/ui/session_service.h>

// C apis
#include <ubuntu/application/ui/ubuntu_application_ui.h>

#include <set>

namespace
{

struct MockPhysicalDisplayInfo : public ubuntu::application::ui::PhysicalDisplayInfo
{
    MockPhysicalDisplayInfo() {}

    int dpi()
    {
        return 96;
    }

    int horizontal_resolution()
    {
        return 1024;
    }

    int vertical_resolution()
    {
        return 768;
    }
};

struct MockSession : public ubuntu::application::ui::Session
{
    MockSession()
    {
    }

    const ubuntu::application::ui::PhysicalDisplayInfo::Ptr& physical_display_info(
        ubuntu::application::ui::PhysicalDisplayIdentifier id)
    {
        static ubuntu::application::ui::PhysicalDisplayInfo::Ptr display(
            new MockPhysicalDisplayInfo());

        return display;
    }

    const ubuntu::application::ui::Surface::Ptr& create_surface(
        const ubuntu::application::ui::SurfaceProperties& props,
        const ubuntu::application::ui::input::Listener::Ptr& listener)
    {
        (void) props;
        (void) listener;
    }

    const ubuntu::application::ui::Surface::Ptr& destroy_surface(
        const ubuntu::application::ui::SurfaceProperties& props)
    {
        (void) props;
    }

    EGLNativeDisplayType to_native_display_type()
    {
        return 0;
    }
};

struct MockSessionService : public ubuntu::ui::SessionService
{
    MockSessionService()
    {
    }

    const ubuntu::application::ui::Session::Ptr& start_a_new_session(const ubuntu::application::ui::SessionCredentials& cred)
    {
        (void) cred;
        static ubuntu::application::ui::Session::Ptr session(new MockSession());
        return session;
    }
};

struct MockSurface : public ubuntu::application::ui::Surface
{
    MockSurface(const ubuntu::application::ui::input::Listener::Ptr& listener)
        : ubuntu::application::ui::Surface(listener)
    {
    }

    bool is_visible() const
    {
        return true;
    }

    void set_visible(bool visible)
    {
        (void) visible;
    }

    void set_alpha(float alpha)
    {
        (void) alpha;
    }

    float alpha() const
    {
        return 1.f;
    }

    void move_to(int x, int y)
    {
        (void) x;
        (void) y;
    }

    void move_by(int dx, int dy)
    {
        (void) dx;
        (void) dy;
    }

    // Bind to EGL/GL rendering API
    EGLNativeWindowType to_native_window_type()
    {
        return 0;
    }
};

struct MockSurfaceFactory : public ubuntu::application::ui::SurfaceFactory
{
    ubuntu::application::ui::Surface::Ptr create_surface(
        const ubuntu::application::ui::SurfaceProperties& props,
        const ubuntu::application::ui::input::Listener::Ptr& listener)
    {
        static ubuntu::application::ui::Surface::Ptr surface(new MockSurface(listener));
        return surface;
    }
};

struct MockSetup : public ubuntu::application::ui::Setup
{
    ubuntu::application::ui::StageHint stage_hint()
    {
        return ubuntu::application::ui::main_stage;
    }

    ubuntu::application::ui::FormFactorHintFlags form_factor_hint()
    {
        return ubuntu::application::ui::desktop_form_factor;
    }
};

}

// We need to inject some platform specific symbols here.
namespace ubuntu
{
namespace application
{
namespace ui
{
const ubuntu::application::ui::SurfaceFactory::Ptr& ubuntu::application::ui::SurfaceFactory::instance()
{
    static ubuntu::application::ui::SurfaceFactory::Ptr session(new MockSurfaceFactory());
    return session;
}

void init(int argc, char** argv)
{
    (void) argc;
    (void) argv;
}

const ubuntu::application::ui::Setup::Ptr& ubuntu::application::ui::Setup::instance()
{
    static ubuntu::application::ui::Setup::Ptr session(new MockSetup());
    return session;
}

}
}
namespace ui
{
const ubuntu::ui::SessionService::Ptr& ubuntu::ui::SessionService::instance()
{
    static ubuntu::ui::SessionService::Ptr instance(new MockSessionService());
    return instance;
}
}
}
