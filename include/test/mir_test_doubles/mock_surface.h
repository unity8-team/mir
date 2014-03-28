/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_SURFACE_H_
#define MIR_TEST_DOUBLES_MOCK_SURFACE_H_

#include "src/server/scene/basic_surface.h"
#include "src/server/report/null_report_factory.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockSurface : public scene::BasicSurface
{
    MockSurface() :
        scene::BasicSurface(
            frontend::SurfaceId{},
            {},
            {{},{}},
            true,
            {},
            {},
            {},
            {},
            mir::report::null_scene_report())
    {
    }

    ~MockSurface() noexcept {}

    MOCK_METHOD0(hide, void());
    MOCK_METHOD0(show, void());
    MOCK_METHOD0(visible, bool());
    MOCK_METHOD1(raise, void(std::shared_ptr<scene::SurfaceRanker> const&));

    MOCK_METHOD0(force_requests_to_complete, void());
    MOCK_METHOD0(advance_client_buffer, std::shared_ptr<graphics::Buffer>());

    MOCK_CONST_METHOD0(size, geometry::Size());
    MOCK_CONST_METHOD0(pixel_format, MirPixelFormat());

    MOCK_CONST_METHOD0(supports_input, bool());
    MOCK_CONST_METHOD0(client_input_fd, int());

    MOCK_METHOD2(configure, int(MirSurfaceAttrib, int));
    MOCK_METHOD1(take_input_focus, void(std::shared_ptr<shell::InputTargeter> const&));
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_MOCK_SURFACE_H_
