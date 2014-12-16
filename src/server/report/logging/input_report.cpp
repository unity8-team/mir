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

#include "input_report.h"

#include "mir/report/legacy_input_report.h"
#include "mir/logging/logger.h"
#include "mir/logging/input_timestamp.h"

#include "std/MirLog.h"
#include <std/Log.h>


#include <sstream>
#include <cstring>
#include <mutex>

namespace mrl = mir::report::logging;
namespace ml = mir::logging;
namespace mrli = mir::report::legacy_input;

namespace
{
char const* const component = "android-input";

class LegacyInputReport;

std::mutex mutex;
std::shared_ptr<LegacyInputReport> the_legacy_input_report;

class LegacyInputReport
{
public:
    LegacyInputReport(std::shared_ptr<ml::Logger> const& logger) :
        logger(logger)
    {
    }

    void log(int prio, char const* buffer)
    {
        switch (prio)
        {
        case ANDROID_LOG_UNKNOWN:
        case ANDROID_LOG_DEFAULT:
        case ANDROID_LOG_VERBOSE:
        case ANDROID_LOG_DEBUG:
            logger->log(ml::Severity::debug, buffer, component);
            break;

        case ANDROID_LOG_INFO:
            logger->log(ml::Severity::informational, buffer, component);
            break;

        case ANDROID_LOG_WARN:
            logger->log(ml::Severity::warning, buffer, component);
            break;

        case ANDROID_LOG_ERROR:
            logger->log(ml::Severity::error, buffer, component);
        };
    }

private:
    std::shared_ptr<ml::Logger> const logger;
};

void my_write_to_log(int prio, char const* buffer)
{
    std::unique_lock<std::mutex> lock(mutex);
    the_legacy_input_report->log(prio, buffer);
}
}


void mrli::initialize(std::shared_ptr<ml::Logger> const& logger)
{
    std::unique_lock<std::mutex> lock(mutex);
    ::the_legacy_input_report = std::make_shared<LegacyInputReport>(logger);

    mir::write_to_log = my_write_to_log;
}


mrl::InputReport::InputReport(const std::shared_ptr<ml::Logger>& logger)
    : logger(logger)
{
}

const char* mrl::InputReport::component()
{
    static const char* s = "input";
    return s;
}

void mrl::InputReport::open_input_device(char const* device)
{
    std::stringstream ss;

    ss << "Opened input device: " << device;

    logger->log(ml::Severity::informational, ss.str(), component());
}

void mrl::InputReport::failure_opening_input_device(char const* device)
{
    std::stringstream ss;

    ss << "Failed to open input device: " << device;

    logger->log(ml::Severity::error, ss.str(), component());
}

void mrl::InputReport::received_event_from_kernel(int64_t when, int type, int code, int value)
{
    std::stringstream ss;

    ss << "Received event"
       << " time=" << ml::input_timestamp(when)
       << " type=" << type
       << " code=" << code
       << " value=" << value;

    logger->log(ml::Severity::informational, ss.str(), component());
}

void mrl::InputReport::published_key_event(int dest_fd, uint32_t seq_id, int64_t event_time)
{
    std::stringstream ss;

    ss << "Published key event"
       << " seq_id=" << seq_id
       << " time=" << ml::input_timestamp(event_time)
       << " dest_fd=" << dest_fd;

    logger->log(ml::Severity::informational, ss.str(), component());
}

void mrl::InputReport::published_motion_event(int dest_fd, uint32_t seq_id, int64_t event_time)
{
    std::stringstream ss;

    ss << "Published motion event"
       << " seq_id=" << seq_id
       << " time=" << ml::input_timestamp(event_time)
       << " dest_fd=" << dest_fd;

    logger->log(ml::Severity::informational, ss.str(), component());
}

void mrl::InputReport::received_event_finished_signal(int src_fd, uint32_t seq_id)
{
    std::stringstream ss;

    ss << "Received event finished"
       << " seq_id=" << seq_id
       << " src_fd=" << src_fd;

    logger->log(ml::Severity::informational, ss.str(), component());
}
