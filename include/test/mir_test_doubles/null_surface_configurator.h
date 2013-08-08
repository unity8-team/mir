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

#ifndef MIR_TEST_DOUBLES_NULL_SURFACE_CONFIGURATOR_H_
#define MIR_TEST_DOUBLES_NULL_SURFACE_CONFIGURATOR_H_

#include "mir/shell/surface_configurator.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct NullSurfaceConfigurator : public shell::SurfaceConfigurator
{
    int select_attribute_value(shell::Surface const&,
                               MirSurfaceAttrib, int requested_value)
    {
        return requested_value;
    }
    void attribute_set(shell::Surface const&,
                       MirSurfaceAttrib, int)
    {
    }
};

}
}
}

#endif /* MIR_TEST_DOUBLES_NULL_SURFACE_CONFIGURATOR_H_ */
