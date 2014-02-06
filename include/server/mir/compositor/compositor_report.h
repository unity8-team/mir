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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef MIR_COMPOSITOR_COMPOSITOR_REPORT_H_
#define MIR_COMPOSITOR_COMPOSITOR_REPORT_H_

#include "mir_toolkit/common.h" //MirPixelFormat

#include <memory>
#include <string>

namespace mir
{
namespace geometry
{
class Size;
}
namespace logging
{
class Logger;
}
namespace compositor
{

class CompositorReport
{
public:
    typedef const void* SubCompositorId;  // e.g. thread/display buffer ID
    typedef const void* GLRendererId;  // e.g. thread/display buffer ID
    virtual void added_display(int width, int height, int x, int y, SubCompositorId id = 0) = 0;
    virtual void began_frame(SubCompositorId id = 0) = 0;
    virtual void finished_frame(bool bypassed, SubCompositorId id = 0) = 0;
    virtual void began_render(GLRendererId id, uint32_t buffer_id, std::string const& name, geometry::Size const& size,
                              MirPixelFormat format, float alpha) = 0;
    virtual void finished_render(GLRendererId id, uint32_t buffer_id) = 0;
    virtual void started() = 0;
    virtual void stopped() = 0;
    virtual void scheduled() = 0;
protected:
    CompositorReport() = default;
    virtual ~CompositorReport() = default;
    CompositorReport(CompositorReport const&) = delete;
    CompositorReport& operator=(CompositorReport const&) = delete;
};

class NullCompositorReport : public CompositorReport
{
public:
    NullCompositorReport() {}
    NullCompositorReport(std::shared_ptr<mir::logging::Logger> const&) {}
    void added_display(int width, int height, int x, int y, SubCompositorId id) override;
    void began_frame(SubCompositorId id) override;
    void finished_frame(bool bypassed, SubCompositorId id) override;
    void began_render(GLRendererId id, uint32_t buffer_id, std::string const& name, geometry::Size const& size,
                      MirPixelFormat format, float alpha) override;
    void finished_render(GLRendererId id, uint32_t buffer_id) override;
    void started() override;
    void stopped() override;
    void scheduled() override;
};

} // namespace compositor
} // namespace mir

#endif // MIR_COMPOSITOR_COMPOSITOR_REPORT_H_
