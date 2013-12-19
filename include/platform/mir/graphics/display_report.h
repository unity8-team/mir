/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_GRAPHICS_DISPLAY_REPORT_H_
#define MIR_GRAPHICS_DISPLAY_REPORT_H_

#include <EGL/egl.h>

namespace mir
{
namespace graphics
{

class DisplayReport
{
public:
    virtual void report_success(bool success, char const* what) = 0;
    virtual void report_egl_configuration(EGLDisplay disp, EGLConfig cfg) = 0;
    /* gbm specific */
    virtual void report_drm_master_failure(int error) = 0;
    /* android specific */
    virtual void report_hwc_composition_in_use(int major, int minor) = 0;
    virtual void report_gpu_composition_in_use() = 0;

protected:
    DisplayReport() = default;
    virtual ~DisplayReport() { /* TODO: make nothrow */ }
    DisplayReport(const DisplayReport&) = delete;
    DisplayReport& operator=(const DisplayReport&) = delete;
};

}
}

#endif /* MIR_GRAPHICS_DISPLAY_REPORT_H_ */
