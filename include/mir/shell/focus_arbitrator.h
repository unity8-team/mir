/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SHELL_FOCUS_ARBITRATOR_H_
#define MIR_SHELL_FOCUS_ARBITRATOR_H_

#include <memory>

namespace mir
{
namespace shell
{
class Session;

class FocusArbitrator
{
public:
    virtual ~FocusArbitrator() {}

    virtual bool request_focus(Session &session) = 0;

protected:
    FocusArbitrator() = default;
    FocusArbitrator(const FocusArbitrator&) = delete;
    FocusArbitrator& operator=(const FocusArbitrator&) = delete;
};

}
}


#endif // MIR_SHELL_FOCUS_SEQUENCE_H_
