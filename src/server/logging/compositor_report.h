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

#ifndef MIR_LOGGING_COMPOSITOR_REPORT_H_
#define MIR_LOGGING_COMPOSITOR_REPORT_H_

#include "mir/compositor/compositor_report.h"
#include "mir/time/clock.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>

namespace mir
{
namespace logging
{

class Logger;

class CompositorReport : public mir::compositor::CompositorReport
{
public:
    CompositorReport(std::shared_ptr<Logger> const& logger,
                     std::shared_ptr<time::Clock> const& clock);
    void added_display(int width, int height, int x, int y, SubCompositorId id);
    void began_frame(SubCompositorId id);
    void finished_frame(bool bypassed, SubCompositorId id);
    void began_render(GLRendererId id, uint32_t buffer_id, std::string const& name,
                                             geometry::Size const& size, MirPixelFormat format, float alpha) override;
    void finished_render(GLRendererId id, uint32_t buffer_id) override;
    void started();
    void stopped();
    void scheduled();

private:
    std::shared_ptr<Logger> const logger;
    std::shared_ptr<time::Clock> const clock;

    typedef time::Timestamp TimePoint;
    TimePoint now() const;

    struct Instance
    {
        TimePoint start_of_frame;
        TimePoint end_of_frame;
        TimePoint total_time_sum;
        TimePoint frame_time_sum;
        TimePoint latency_sum;
        long nframes = 0;
        long nbypassed = 0;
        bool prev_bypassed = false;

        TimePoint last_reported_total_time_sum;
        TimePoint last_reported_frame_time_sum;
        TimePoint last_reported_latency_sum;
        long last_reported_nframes = 0;
        long last_reported_bypassed = 0;

        void log(Logger& logger, SubCompositorId id);
    };

    std::mutex mutex; // Protects the following...
    std::unordered_map<SubCompositorId, Instance> instance;
    TimePoint last_scheduled;
    TimePoint last_report;
};

} // namespace logging
} // namespace mir

#endif // MIR_LOGGING_COMPOSITOR_REPORT_H_
