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
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_SCENE_INPUT_REGISTRAR_OBSERVER_H_
#define MIR_SCENE_INPUT_REGISTRAR_OBSERVER_H_

#include "mir/input/input_reception_mode.h"

#include <memory>

namespace mir
{
namespace input
{
class SessionTarget;
class InputChannel;
class Surface;
}

namespace scene
{

/// An interface used to register input targets and take care of input assosciation (i.e.
/// create input channels).
class InputRegistrarObserver
{
public:
    virtual ~InputRegistrarObserver() = default;

    virtual void input_channel_opened(std::shared_ptr<input::InputChannel> const& opened_channel,
                                      std::shared_ptr<input::Surface> const& info,
                                      input::InputReceptionMode input_mode) = 0;
    virtual void input_channel_closed(std::shared_ptr<input::InputChannel> const& closed_channel) = 0;

protected:
    InputRegistrarObserver() = default;
    InputRegistrarObserver(InputRegistrarObserver const&) = delete;
    InputRegistrarObserver& operator=(InputRegistrarObserver const&) = delete;
};

}
} // namespace mir

#endif
