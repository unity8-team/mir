/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "session_management_context.h"

#include "mir/sessions/surface.h"
#include "mir/sessions/surface_creation_parameters.h"
#include "mir/sessions/session.h"
#include "mir/sessions/registration_order_focus_sequence.h"
#include "mir/sessions/single_visibility_focus_mechanism.h"
#include "mir/sessions/session_container.h"
#include "mir/sessions/session_store.h"
#include "mir/sessions/surface_factory.h"
#include "mir/graphics/display.h"
#include "mir/server_configuration.h"

namespace msess = mir::sessions;

namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace geom = mir::geometry;
namespace mtc = mir::test::cucumber;

namespace mir
{
namespace test
{
namespace cucumber
{

static const geom::Width default_view_width = geom::Width{1600};
static const geom::Height default_view_height = geom::Height{1400};

static const geom::Size default_view_size = geom::Size{default_view_width,
                                                       default_view_height};
static const geom::Rectangle default_view_area = geom::Rectangle{geom::Point(),
                                                                 default_view_size};

struct DummySurface : public msess::Surface
{
    explicit DummySurface() {}
    virtual ~DummySurface() {}
    
    virtual void hide() {}
    virtual void show() {}
    virtual void destroy() {}
    virtual void shutdown() {}
    
    virtual geom::Size size() const
    {
        return geom::Size();
    }
    virtual geom::PixelFormat pixel_format() const
    {
        return geom::PixelFormat();
    }

    virtual void advance_client_buffer() {}
    virtual std::shared_ptr<mc::Buffer> client_buffer() const
    {
        return std::shared_ptr<mc::Buffer>();
    }
};

struct SizedDummySurface : public DummySurface
{
    explicit SizedDummySurface(geom::Size const& size)
        : surface_size(size)
    {
    }
    
    geom::Size size() const
    {
        return surface_size;
    }
    
    geom::Size const surface_size;
};

struct DummySurfaceFactory : public msess::SurfaceFactory
{
    explicit DummySurfaceFactory()
    {
    }

    std::shared_ptr<msess::Surface> create_surface(const msess::SurfaceCreationParameters& params)
    {
        auto name = params.name;
        return std::make_shared<SizedDummySurface>(params.size);
    }
};

class SizedDisplay : public mg::Display
{
public:
    explicit SizedDisplay(geom::Rectangle const& view_area = default_view_area)
        : area(view_area)
    {
    }

    geom::Rectangle view_area() const
    {
        return area;
    }

    void set_view_area(geom::Rectangle const& new_view_area)
    {
        area = new_view_area;
    }

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f)
    {
        (void)f;
    }
    
    std::shared_ptr<mg::DisplayConfiguration> configuration()
    {
        return std::shared_ptr<mg::DisplayConfiguration>();
    }
    
    geom::Rectangle area;
};

}
}
} // namespace mir

namespace
{
    struct DummyServerConfiguration : mir::DefaultServerConfiguration
    {
    } server_configuration;
}

mtc::SessionManagementContext::SessionManagementContext() :
    view_area(std::make_shared<mtc::SizedDisplay>()),
    session_store(server_configuration.the_session_store(
        std::make_shared<mtc::DummySurfaceFactory>(),
        view_area))
{
}

mtc::SessionManagementContext::SessionManagementContext(ServerConfiguration& server_configuration) :
    view_area(std::make_shared<mtc::SizedDisplay>()),
    session_store(server_configuration.the_session_store(
        std::make_shared<mtc::DummySurfaceFactory>(),
        view_area))
{
}

// TODO: This will be less awkward with the ApplicationWindow class.
bool mtc::SessionManagementContext::open_window_consuming(std::string const& window_name)
{
    auto const params = msess::a_surface().of_name(window_name);
    auto session = session_store->open_session(window_name);
    auto const surface_id = session->create_surface(params);

    open_windows[window_name] = std::make_tuple(session, surface_id);

    return true;
}

bool mtc::SessionManagementContext::open_window_with_size(std::string const& window_name,
                                                          geom::Size const& size)
{
    auto const params = msess::a_surface().of_name(window_name).of_size(size);
    auto session = session_store->open_session(window_name);
    auto const surface_id = session->create_surface(params);

    open_windows[window_name] = std::make_tuple(session, surface_id);

    return true;
}

geom::Size mtc::SessionManagementContext::get_window_size(std::string const& window_name)
{
    auto window = open_windows[window_name];
    auto session = std::get<0>(window);
    auto surface_id = std::get<1>(window);

    return session->get_surface(surface_id)->size();
}

void mtc::SessionManagementContext::set_view_area(geom::Rectangle const& new_view_region)
{
    view_area->set_view_area(new_view_region);
}

