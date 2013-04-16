/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_SHELL_SURFACE_SOURCE_H_
#define MIR_SHELL_SURFACE_SOURCE_H_

#include "mir/shell/surface_factory.h"

#include <memory>

namespace mir
{
namespace input
{
class InputChannelFactory;
}

namespace shell
{
class SurfaceBuilder;

class SurfaceSource : public SurfaceFactory
{
public:
    explicit SurfaceSource(std::shared_ptr<SurfaceBuilder> const& surface_builder, std::shared_ptr<input::InputChannelFactory> const& input_factory);
    virtual ~SurfaceSource() {}

    std::shared_ptr<Surface> create_surface(const frontend::SurfaceCreationParameters& params);

protected:
    SurfaceSource(const SurfaceSource&) = delete;
    SurfaceSource& operator=(const SurfaceSource&) = delete;

private:
    std::shared_ptr<SurfaceBuilder> const surface_builder;
    std::shared_ptr<input::InputChannelFactory> const input_factory;
};

}
}

#endif // MIR_SHELL_SURFACE_SOURCE_H_
