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

#ifndef MIR_SHELL_WRAPPING_SERVER_CONFIGURATION_H_
#define MIR_SHELL_WRAPPING_SERVER_CONFIGURATION_H_

#include <functional>
#include <memory>

namespace mir
{
namespace scene { class SurfaceCoordinator; }

namespace shell
{
typedef std::function<std::shared_ptr<scene::SurfaceCoordinator>(std::shared_ptr<scene::SurfaceCoordinator> const& wrapped)> WrapSurfaceCoordinator;

template<class BaseConfiguration>
class WrappingServerConfiguration : public BaseConfiguration
{
public:
    using BaseConfiguration::BaseConfiguration;

    std::shared_ptr<scene::SurfaceCoordinator> the_surface_coordinator() override
    {
        return this->surface_coordinator([&]()
            {
                auto const wrapped = BaseConfiguration::the_surface_coordinator();
                return wrap_surface_coordinator(wrapped);
            });
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

#endif /* MIR_SHELL_WRAPPING_SERVER_CONFIGURATION_H_ */
