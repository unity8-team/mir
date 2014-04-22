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

#ifndef MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_OBSERVER_H_
#define MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_OBSERVER_H_

#include "mir/scene/input_registrar_observer.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubInputRegistrarObserver : public scene::InputRegistrarObserver
{
    void input_channel_opened(std::shared_ptr<input::InputChannel> const&,
                              std::shared_ptr<input::Surface> const&,
                              input::InputReceptionMode)
    {
    }
    void input_channel_closed(std::shared_ptr<input::InputChannel> const&)
    {
    }
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_OBSERVER_H_
