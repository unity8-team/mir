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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/shell/unauthorized_display_arbitrator.h"
#include <boost/throw_exception.hpp>

namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace mg = mir::graphics;

msh::UnauthorizedDisplayArbitrator::UnauthorizedDisplayArbitrator(std::shared_ptr<frontend::DisplayArbitrator> const& arbitrator)
    : arbitrator(arbitrator)
{
}

std::shared_ptr<mg::DisplayConfiguration> msh::UnauthorizedDisplayArbitrator::active_configuration()
{
    return arbitrator->active_configuration();
}

void msh::UnauthorizedDisplayArbitrator::configure(
    std::shared_ptr<mf::Session> const&, std::shared_ptr<mg::DisplayConfiguration> const&)
{
    BOOST_THROW_EXCEPTION(std::runtime_error("not authorized to apply display configurations"));
}

void msh::UnauthorizedDisplayArbitrator::ensure_display_powered(std::shared_ptr<mf::Session> const&)
{
    BOOST_THROW_EXCEPTION(std::runtime_error("not authorized to apply display configurations"));
}
