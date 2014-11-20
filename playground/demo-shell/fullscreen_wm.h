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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_EXAMPLES_FULLSCREEN_WM_H_
#define MIR_EXAMPLES_FULLSCREEN_WM_H_

#include "mir/shell/window_manager.h"

#include <memory>

namespace mir
{
namespace shell
{
class DisplayLayout;
}
namespace examples
{

// TODO: Rename when the other "examples::WindowManager" is gone
class FullscreenWM : public shell::WindowManager
{
public:
    FullscreenWM(std::shared_ptr<shell::DisplayLayout> const& display_layout);
    ~FullscreenWM() = default;
    
    scene::SurfaceCreationParameters place(scene::Session const&, scene::SurfaceCreationParameters const& request_parameters);

protected:
    FullscreenWM(FullscreenWM const&) = delete;
    FullscreenWM& operator=(FullscreenWM const&) = delete;

private:
    std::shared_ptr<shell::DisplayLayout> const display_layout;
};

}
} // namespace mir

#endif // MIR_EXAMPLES_FULLSCREEN_WM_H_
