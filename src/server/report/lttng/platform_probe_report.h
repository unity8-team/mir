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

#ifndef MIR_REPORT_LTTNG_PLATFORM_PROBE_REPORT_H_
#define MIR_REPORT_LTTNG_PLATFORM_PROBE_REPORT_H_

#include "server_tracepoint_provider.h"

#include "mir/graphics/platform_probe_report.h"

namespace mir
{
namespace report
{
namespace lttng
{
class PlatformProbeReport : public graphics::PlatformProbeReport
{
public:
    void module_probed(graphics::ModuleProperties const& module,
                       graphics::PlatformPriority probe_value) override;
    void invalid_module_probed(std::exception const& error) override;
    void module_selected(graphics::ModuleProperties const& module) override;

private:
    ServerTracepointProvider tp_provider;
};
}
}
}

#endif /* MIR_REPORT_LTTNG_PLATFORM_PROBE_REPORT_H_ */
