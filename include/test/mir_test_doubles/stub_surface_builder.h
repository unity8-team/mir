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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_
#define MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_

#include "src/server/scene/surface_builder.h"
#include "src/server/scene/surface.h"
#include "mir/scene/scene_report.h"
#include "mir/shell/surface_creation_parameters.h"

#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/mock_surface_state.h"

namespace mir
{
namespace test
{
namespace doubles
{

class StubSurfaceBuilder : public scene::SurfaceBuilder
{
public:
    StubSurfaceBuilder() :
        buffer_stream(std::make_shared<StubBufferStream>()),
        dummy_surface()
    {
    }

    std::weak_ptr<scene::BasicSurface> create_surface(shell::Session*, shell::SurfaceCreationParameters const&)
    {
        auto state = std::make_shared<MockSurfaceState>();
        dummy_surface = std::make_shared<scene::Surface>(
            state, buffer_stream,
            std::shared_ptr<input::InputChannel>(),
            report);

        return dummy_surface;
    }

    void destroy_surface(std::weak_ptr<scene::BasicSurface> const& )
    {
    }
private:
    std::shared_ptr<compositor::BufferStream> const buffer_stream;
    std::shared_ptr<scene::BasicSurface>  dummy_surface;
    std::shared_ptr<scene::SceneReport> report = std::make_shared<scene::NullSceneReport>();

};
}
}
}


#endif /* MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_ */
