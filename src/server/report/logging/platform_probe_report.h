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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_REPORT_LOGGING_PLATFORM_PROBE_REPORT_H_
#define MIR_REPORT_LOGGING_PLATFORM_PROBE_REPORT_H_

#include "mir/graphics/platform_probe_report.h"

#include <memory>

namespace mir
{
namespace logging
{
class Logger;
}
namespace report
{
namespace logging
{

class PlatformProbeReport : public graphics::PlatformProbeReport
{
public:
    PlatformProbeReport(std::shared_ptr<mir::logging::Logger> const& logger);

    void module_probed(const graphics::ModuleProperties &module, graphics::PlatformPriority probe_value);
    void invalid_module_probed(const std::exception &error);
    void module_selected(const graphics::ModuleProperties &module);

private:
    std::shared_ptr<mir::logging::Logger> const logger;
};

}
}
}


#endif /* MIR_REPORT_LOGGING_PLATFORM_PROBE_REPORT_H_ */
