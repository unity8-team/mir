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

#ifndef MIR_REPORT_LOGGING_MESSAGE_PROCESSOR_REPORT_H_
#define MIR_REPORT_LOGGING_MESSAGE_PROCESSOR_REPORT_H_

#include "mir/frontend/message_processor_report.h"
#include "mir/time/clock.h"

#include <memory>
#include <mutex>
#include <unordered_map>

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
namespace detail
{
struct InvocationDetails
{
    time::Timestamp start;
    std::string method;
    std::string exception;
};

typedef std::unordered_map<int, InvocationDetails> Invocations;

struct MediatorDetails
{
    Invocations current_invocations;
};

typedef std::unordered_map<void const*, MediatorDetails> Mediators;
}

class MessageProcessorReport : public mir::frontend::MessageProcessorReport
{
public:
    MessageProcessorReport(
        std::shared_ptr<mir::logging::Logger> const& log,
        std::shared_ptr<time::Clock> const& clock);

    void received_invocation(void const* mediator, int id, std::string const& method);

    void completed_invocation(void const* mediator, int id, bool result);

    void unknown_method(void const* mediator, int id, std::string const& method);

    void exception_handled(void const* mediator, int id, std::exception const& error);

    void exception_handled(void const* mediator, std::exception const& error);

    ~MessageProcessorReport() noexcept(true);

private:
    std::shared_ptr<mir::logging::Logger> const log;
    std::shared_ptr<time::Clock> const clock;
    std::mutex mutex;
    detail::Mediators mediators;
};
}
}
}

#endif /* MIR_REPORT_LOGGING_MESSAGE_PROCESSOR_REPORT_H_ */
