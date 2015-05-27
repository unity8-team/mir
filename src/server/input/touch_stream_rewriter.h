/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef MIR_INPUT_TOUCH_STREAM_REWRITER_H_
#define MIR_INPUT_TOUCH_STREAM_REWRITER_H_

#include "mir/input/input_dispatcher.h"

#include <memory>

namespace mir
{
namespace input
{
class TouchStreamRewriter : public mir::input::InputDispatcher
{
public:
    TouchStreamRewriter(std::shared_ptr<InputDispatcher> const& next_dispatcher);

    // InputDispatcher
    void dispatch(MirEvent const& event) override;
    void start() override;
    void stop() override;
    
private:
    std::shared_ptr<InputDispatcher> const next_dispatcher;
};

}
}

#endif // MIR_INPUT_TOUCH_STREAM_REWRITER_H_
