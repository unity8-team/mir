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
        scene::SurfaceCreationParameters const& params,
        scene::Session* session) override
    {
        return wrapped->add_surface(params, session);
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

    WrappingServerConfiguration& wrap_surface_coordinator_using(WrapSurfaceCoordinator const& wrap)
    {
        wrap_surface_coordinator = wrap;
        return *this;
    }

    template <class Wrapper>
    WrappingServerConfiguration& wrap_surface_coordinator_with()
    {
        return wrap_surface_coordinator_using(
            [](std::shared_ptr<scene::SurfaceCoordinator> const& wrapped)
            -> std::shared_ptr<scene::SurfaceCoordinator>
            {
                return std::make_shared<Wrapper>(wrapped);
            });
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
#include <gmock/gmock.h>

namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mtf = mir_test_framework;

using namespace testing;

namespace
{
using MyConfig = msh::WrappingServerConfiguration<mtf::StubbedServerConfiguration>;

struct WrappingServerConfiguration : Test
{
    MyConfig config;
};
}

TEST_F(WrappingServerConfiguration, can_set_surface_coordinator_wrapper_type)
{
    using MySurfaceCoordinator = msh::SurfaceCoordinatorWrapper;

    config.wrap_surface_coordinator_with<MySurfaceCoordinator>();

    auto const surface_coordinator = config.the_surface_coordinator();
    auto const my_surface_coordinator = std::dynamic_pointer_cast<MySurfaceCoordinator>(surface_coordinator);

    EXPECT_THAT(my_surface_coordinator, Ne(nullptr));
}

TEST_F(WrappingServerConfiguration, can_set_surface_coordinator_wrapper_functor)
{
    using MySurfaceCoordinator = msh::SurfaceCoordinatorWrapper;

    config.wrap_surface_coordinator_using(
        [](std::shared_ptr<ms::SurfaceCoordinator> const& wrapped)
        -> std::shared_ptr<ms::SurfaceCoordinator>
        {
            return std::make_shared<MySurfaceCoordinator>(wrapped);
        });

    auto const surface_coordinator = config.the_surface_coordinator();
    auto const my_surface_coordinator = std::dynamic_pointer_cast<MySurfaceCoordinator>(surface_coordinator);

    EXPECT_THAT(my_surface_coordinator, Ne(nullptr));
}

TEST_F(WrappingServerConfiguration, can_override_surface_coordinator_methods)
{
    struct MySurfaceCoordinator : msh::SurfaceCoordinatorWrapper
    {
        using msh::SurfaceCoordinatorWrapper::SurfaceCoordinatorWrapper;
        MOCK_METHOD1(raise, void(std::weak_ptr<ms::Surface> const&));
    };

    std::shared_ptr<MySurfaceCoordinator> my_surface_coordinator;

    config.wrap_surface_coordinator_using(
        [&](std::shared_ptr<ms::SurfaceCoordinator> const& wrapped)
        -> std::shared_ptr<ms::SurfaceCoordinator>
        {
            return my_surface_coordinator = std::make_shared<MySurfaceCoordinator>(wrapped);
        });

    auto const surface_coordinator = config.the_surface_coordinator();

    ASSERT_THAT(my_surface_coordinator, Ne(nullptr));

    EXPECT_CALL(*my_surface_coordinator, raise(_)).Times(1);
    surface_coordinator->raise({});
}
