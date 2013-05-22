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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/run_mir.h"
#include "mir/report_exception.h"

#include "mir/default_server_configuration.h"
#include "mir/compositor/bypass_compositing_strategy.h"

namespace mc = mir::compositor;

namespace
{
class CompositionBypassConfiguration : public mir::DefaultServerConfiguration
{
public:
//    using mir::DefaultServerConfiguration::DefaultServerConfiguration;
    CompositionBypassConfiguration(int argc, char const* argv[]) :
        mir::DefaultServerConfiguration(argc, argv) {}

    std::shared_ptr<mc::CompositingStrategy>
    the_compositing_strategy()
    {
        return compositing_strategy(
            [this]{ return std::make_shared<mc::BypassCompositingStrategy>(); });
    }
};
}

int main(int argc, char const** argv)
try
{
    CompositionBypassConfiguration conf{argc, argv};

    mir::run_mir(conf, [&](mir::DisplayServer&){});

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
