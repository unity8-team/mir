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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_SCENE_H_
#define MIR_TEST_DOUBLES_MOCK_SCENE_H_

#include "mir/compositor/scene.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockScene : public compositor::Scene
{
public:
    MockScene()
    {
        ON_CALL(*this, generate_renderable_list())
            .WillByDefault(testing::Return(graphics::RenderableList{}));
    }
    MOCK_CONST_METHOD0(generate_renderable_list, graphics::RenderableList());

    MOCK_METHOD1(add_change_callback, scene::ObserverID(std::function<void()> const&));
    MOCK_METHOD1(remove_change_callback, void(scene::ObserverID));

    MOCK_METHOD0(lock, void());
    MOCK_METHOD0(unlock, void());
};

} // namespace doubles
} // namespace test
} // namespace mir

#endif /* MIR_TEST_DOUBLES_MOCK_SCENE_H_ */
