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

#ifndef MIR_SHELL_INPUT_INJECTER_H_
#define MIR_SHELL_INPUT_INJECTER_H_

#include "mir_toolkit/event.h"

#include <memory>

namespace mir
{

namespace shell
{
class Surface;

/// An interface used to control the selection of keyboard input focus.
class InputInjecter
{
public:
    virtual ~InputInjecter() = default;
    
    virtual void inject_input(std::shared_ptr<Surface> const& surface,
                              MirEvent const& ev) = 0;

protected:
    InputInjecter() = default;
    InputInjecter(InputInjecter const&) = delete;
    InputInjecter& operator=(InputInjecter const&) = delete;
};

}
} // namespace mir

#endif // MIR_SHELL_INPUT_INJECTER_H_
