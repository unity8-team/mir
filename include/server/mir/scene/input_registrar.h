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

#ifndef MIR_SCENE_INPUT_REGISTRAR_H_
#define MIR_SCENE_INPUT_REGISTRAR_H_

#include "mir/scene/input_registrar_observer.h"

#include <memory>

namespace mir
{
namespace scene
{

/// An interface to subscribe to input target changes
class InputRegistrar : public InputRegistrarObserver
{
public:
    virtual ~InputRegistrar() = default;

    virtual void add_observer(std::shared_ptr<InputRegistrarObserver> const& observer) = 0;
    virtual void remove_observer(std::shared_ptr<InputRegistrarObserver> const& observer) = 0;

protected:
    InputRegistrar() = default;
    InputRegistrar(InputRegistrar const&) = delete;
    InputRegistrar& operator=(InputRegistrar const&) = delete;
};

}
} // namespace mir

#endif // MIR_SCENE_INPUT_REGISTRAR_H_
