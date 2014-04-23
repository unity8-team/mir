/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_H_
#define MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_H_

#include "mir/scene/input_registrar.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubInputRegistrar : public scene::InputRegistrar
{
    void input_channel_opened(std::shared_ptr<input::InputChannel> const& /*channel*/,
                              std::shared_ptr<input::Surface> const& /*surface*/,
                              input::InputReceptionMode /*mode*/) override
    {
    }
    void input_channel_closed(std::shared_ptr<input::InputChannel> const& /*channel*/) override
    {
    }
    void add_observer(std::shared_ptr<InputRegistrarObserver> const& /*observer*/) override
    {
    }
    void remove_observer(std::shared_ptr<InputRegistrarObserver> const& /*observer*/) override
    {
    }
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_STUB_INPUT_REGISTRAR_H_
