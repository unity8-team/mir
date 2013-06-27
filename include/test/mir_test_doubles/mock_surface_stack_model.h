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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */
#ifndef MIR_TEST_DOUBLES_MOCK_SURFACE_STACK_MODEL_H_
#define MIR_TEST_DOUBLES_MOCK_SURFACE_STACK_MODEL_H_

#include "mir/surfaces/surface_stack_model.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockSurfaceStackModel : public surfaces::SurfaceStackModel
{
    MOCK_METHOD2(create_surface, std::weak_ptr<surfaces::Surface>(shell::SurfaceCreationParameters const&, surfaces::DepthId));
    MOCK_METHOD1(destroy_surface, void(std::weak_ptr<surfaces::Surface> const& surface));
};

}
}
}
#endif /* MIR_TEST_DOUBLES_MOCK_SURFACE_STACK_MODEL_H_ */
