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

#include <memory>

namespace mir
{
namespace examples
{

class SoftwareCursorCompositingStrategy : public compositor::DefaultCompositingStrategy
{
public:
    SoftwareCursorCompositingStrategy(std::shared_ptr<compositor::Renderables> const& renderables,
                                      std::shared_ptr<graphics::Renderer> const& renderer);
    ~SoftwareCursorCompositingStrategy() = default;
    
    virtual void render(graphics::DisplayBuffer& diplay_buffer);
    
protected:
    SoftwareCursorCompositingStrategy(SoftwareCursorCompositingStrategy const&) = delete;
    SoftwareCursorCompositingStrategy& operator=(SoftwareCursorCompositingStrategy const&) = delete;
};

}
} // namespace mir

#endif // MIR_EXAMPLES_SOFTWARE_CURSOR_COMPOSITING_STRATEGY_H_
