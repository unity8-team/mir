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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_SCENE_H_
#define MIR_TEST_DOUBLES_STUB_SCENE_H_

#include "mir/compositor/scene.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class StubScene : public compositor::Scene
{
public:
    compositor::SceneElementList scene_elements_for(void const*) override
    {
          return {};
    }
    void rendering_result_for(CompositorID,
                              compositor::SceneElementList const&,
                              compositor::SceneElementList const&) override
    {
    }
    void register_compositor(CompositorID) override {}
    void unregister_compositor(CompositorID) override {}
    void add_observer(std::shared_ptr<scene::Observer> const&) override
    {
    }
    void remove_observer(std::weak_ptr<scene::Observer> const&) override
    {
    }
};

} // namespace doubles
} // namespace test
} // namespace mir

#endif /* MIR_TEST_DOUBLES_STUB_SCENE_H_ */
