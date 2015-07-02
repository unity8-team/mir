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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_DISPLAY_CHANGER_H_
#define MIR_TEST_DOUBLES_STUB_DISPLAY_CHANGER_H_

#include "mir/display_changer.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubDisplayChanger : public mir::DisplayChanger
{
    void configure_for_hardware_change(
        std::shared_ptr<graphics::DisplayConfiguration> const& ,
        SystemStateHandling) override
    {}

    void pause_display_config_processing() override
    {}

    void resume_display_config_processing() override
    {}
    void register_change_callback(std::function<void(graphics::DisplayConfiguration const&)> const&) override
    {}

};

}
}
}

#endif
