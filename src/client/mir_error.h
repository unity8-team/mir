/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_CLIENT_MIR_ERROR_H_
#define MIR_CLIENT_MIR_ERROR_H_

#include "mir_toolkit/client_types.h"

struct MirError
{
public:
    MirError(MirErrorDomain domain, uint32_t code);

    MirErrorDomain domain() const noexcept;
    uint32_t code() const noexcept;

private:
    MirErrorDomain const domain_;
    uint32_t const code_;
};

#endif //MIR_CLIENT_MIR_ERROR_H_
