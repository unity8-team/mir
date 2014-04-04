/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SCENE_SCENETACK_H_
#define MIR_SCENE_SCENETACK_H_

#include "surface_stack_model.h"

#include "mir/compositor/scene.h"
#include "mir/scene/depth_id.h"
#include "mir/input/input_targets.h"

#include <memory>
#include <vector>
#include <mutex>
#include <map>

namespace mir
{
namespace compositor
{
class FilterForScene;
class OperatorForScene;
}

namespace frontend
{
struct SurfaceCreationParameters;
}

namespace input
{
class InputChannelFactory;
class InputChannel;
}

/// Management of Surface objects. Includes the model (SurfaceStack and Surface
/// classes) and controller (SurfaceController) elements of an MVC design.
namespace scene
{
class InputRegistrar;
class BasicSurface;
class SceneReport;

class SurfaceStack : public compositor::Scene, public input::InputTargets, public SurfaceStackModel
{
public:
    explicit SurfaceStack(
        std::shared_ptr<InputRegistrar> const& input_registrar,
        std::shared_ptr<SceneReport> const& report);
    virtual ~SurfaceStack() noexcept(true) {}

    // From Scene
    graphics::RenderableList generate_renderable_list() const;
    virtual void set_change_callback(std::function<void()> const& f);
    //to be deprecated
    virtual void for_each_if(compositor::FilterForScene &filter, compositor::OperatorForScene &op);
    virtual void lock();
    virtual void unlock();
    //end to be deprecated
    
    // From InputTargets
    void for_each(std::function<void(std::shared_ptr<input::InputChannel> const&)> const& callback);

    virtual void remove_surface(std::weak_ptr<Surface> const& surface) override;

    virtual void raise(std::weak_ptr<Surface> const& surface) override;

    void add_surface(
        std::shared_ptr<Surface> const& surface,
        DepthId depth,
        input::InputReceptionMode input_mode) override;

private:
    SurfaceStack(const SurfaceStack&) = delete;
    SurfaceStack& operator=(const SurfaceStack&) = delete;

    void emit_change_notification();

    std::recursive_mutex mutable guard;
    std::shared_ptr<InputRegistrar> const input_registrar;
    std::shared_ptr<SceneReport> const report;
    std::function<void()> const change_cb;

    typedef std::vector<std::shared_ptr<Surface>> Layer;
    std::map<DepthId, Layer> layers_by_depth;

    std::mutex notify_change_mutex;
    std::function<void()> notify_change;
};

}
}

#endif /* MIR_SCENE_SCENETACK_H_ */
