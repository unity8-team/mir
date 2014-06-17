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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SHELL_SINGLE_VISIBILITY_FOCUS_MECHANISM_H_
#define MIR_SHELL_SINGLE_VISIBILITY_FOCUS_MECHANISM_H_

#include "mir/shell/focus_setter.h"

#include <memory>
#include <mutex>

namespace mir
{
namespace scene { class SurfaceCoordinator; class Surface; }

namespace shell
{
class InputTargeter;

class DefaultFocusMechanism : public FocusSetter
{
public:
    explicit DefaultFocusMechanism(std::shared_ptr<InputTargeter> const& input_targeter,
                                   std::shared_ptr<scene::SurfaceCoordinator> const& surface_coordinator);
    virtual ~DefaultFocusMechanism() = default;

    void set_focus_to(std::shared_ptr<scene::Session> const& new_focus);

protected:
    DefaultFocusMechanism(const DefaultFocusMechanism&) = delete;
    DefaultFocusMechanism& operator=(const DefaultFocusMechanism&) = delete;

private:
    std::shared_ptr<InputTargeter> const input_targeter;
    std::shared_ptr<scene::SurfaceCoordinator> const surface_coordinator;

    std::mutex surface_focus_lock;
    std::weak_ptr<scene::Surface> currently_focused_surface;
};

}
}


#endif // MIR_SHELL_SINGLE_VISIBILITY_FOCUS_MECHANISM_H_