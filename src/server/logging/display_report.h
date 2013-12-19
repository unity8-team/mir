/*
 * Copyright © 2012 Canonical Ltd.
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


#ifndef MIR_LOGGING_DISPLAY_REPORTER_H_
#define MIR_LOGGING_DISPLAY_REPORTER_H_

#include "mir/graphics/display_report.h"

#include <memory>

namespace mir
{
namespace logging
{
class Logger;

class DisplayReport : public graphics::DisplayReport
{
  public:
    DisplayReport(const std::shared_ptr<Logger>& logger);
    virtual ~DisplayReport();

    virtual void report_success(bool success, char const* what);
    virtual void report_drm_master_failure(int error);
    virtual void report_hwc_composition_in_use(int major, int minor);
    virtual void report_gpu_composition_in_use();
    virtual void report_egl_configuration(EGLDisplay disp, EGLConfig cfg);

  protected:
    DisplayReport(const DisplayReport&) = delete;
    DisplayReport& operator=(const DisplayReport&) = delete;

  private:
    std::shared_ptr<logging::Logger> logger;
};
}
}

#endif /* MIR_LOGGING_DISPLAY_REPORTER_H_ */
