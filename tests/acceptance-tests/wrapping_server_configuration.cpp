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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/scene/surface_coordinator.h"

namespace mir
{
namespace shell
{
class SurfaceCoordinatorWrapper : public scene::SurfaceCoordinator
{
public:
    explicit SurfaceCoordinatorWrapper(std::shared_ptr<scene::SurfaceCoordinator> const& wrapped) :
    wrapped(wrapped) {}

    std::shared_ptr<scene::Surface> add_surface(
        SurfaceCreationParameters const& params,
        scene::Session* session,
        std::shared_ptr<scene::SurfaceObserver> const& observer) override
    {
        return wrapped->add_surface(params, session, observer);
    }

    void raise(std::weak_ptr<scene::Surface> const& surface) override
    {
        wrapped->raise(surface);
    }

    void remove_surface(std::weak_ptr<scene::Surface> const& surface) override
    {
        wrapped->remove_surface(surface);
    }

protected:
    std::shared_ptr<SurfaceCoordinator> const wrapped;
};

typedef std::function<std::shared_ptr<scene::SurfaceCoordinator>(std::shared_ptr<scene::SurfaceCoordinator> const& wrapped)> WrapSurfaceCoordinator;

template<class BaseConfiguration>
class WrappingServerConfiguration : public BaseConfiguration
{
public:
    using BaseConfiguration::BaseConfiguration;

    std::shared_ptr<scene::SurfaceCoordinator> the_surface_coordinator() override
    {
        auto const wrapped = BaseConfiguration::the_surface_coordinator();
        return wrap_surface_coordinator(wrapped);
    }

    WrappingServerConfiguration& with_wrapping(WrapSurfaceCoordinator const& wrap)
    {
        wrap_surface_coordinator = wrap;
        return *this;
    }

private:
    WrapSurfaceCoordinator wrap_surface_coordinator{
        [](std::shared_ptr<scene::SurfaceCoordinator> const& wrapped)
        -> std::shared_ptr<scene::SurfaceCoordinator>
        { return wrapped; }};
};
}
}

#include "mir_test_framework/stubbed_server_configuration.h"

#include <gtest/gtest.h>

namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mtf = mir_test_framework;

using namespace testing;

using MyConfig = msh::WrappingServerConfiguration<mtf::StubbedServerConfiguration>;

TEST(WrappingServerConfiguration, can_set_surface_coordinator_wrapping)
{
    MyConfig config;

    config.with_wrapping([](std::shared_ptr<ms::SurfaceCoordinator> const& wrapped)
        -> std::shared_ptr<ms::SurfaceCoordinator>
    {
        return std::make_shared<msh::SurfaceCoordinatorWrapper>(wrapped);
    });
}
