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

#ifndef MIR_EXAMPLES_SOFTWARE_CURSOR_COMPOSITING_STRATEGY_H_
#define MIR_EXAMPLES_SOFTWARE_CURSOR_COMPOSITING_STRATEGY_H_

#include "mir/compositor/default_compositing_strategy.h"
#include "mir/input/cursor_listener.h"

#include <memory>
#include <map>

namespace mir
{
namespace examples
{
class GLCursorRenderer;

class SoftwareCursorCompositingStrategy : public compositor::CompositingStrategy, public input::CursorListener
{
public:
    SoftwareCursorCompositingStrategy(std::shared_ptr<compositor::Renderables> const& renderables,
                                      std::shared_ptr<graphics::Renderer> const& renderer);
    ~SoftwareCursorCompositingStrategy() = default;
    
    virtual void cursor_moved_to(float abs_x, float abs_y);
    virtual void render(graphics::DisplayBuffer& diplay_buffer);
    
protected:
    SoftwareCursorCompositingStrategy(SoftwareCursorCompositingStrategy const&) = delete;
    SoftwareCursorCompositingStrategy& operator=(SoftwareCursorCompositingStrategy const&) = delete;

private:
    std::shared_ptr<compositor::Renderables> const renderables;
    std::shared_ptr<graphics::Renderer> const renderer;
    
    // TODO: May need to be one per display buffer?
    std::shared_ptr<GLCursorRenderer> cursor_renderer;

    float cursor_x;
    float cursor_y;
};

}
} // namespace mir

#endif // MIR_EXAMPLES_SOFTWARE_CURSOR_COMPOSITING_STRATEGY_H_
