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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_REPORT_LTTNG_CONNECTOR_REPORT_H_
#define MIR_REPORT_LTTNG_CONNECTOR_REPORT_H_

#include "server_tracepoint_provider.h"

#include "mir/frontend/connector_report.h"

#include <stdexcept>
#include <string>

namespace mir
{
namespace report
{
namespace lttng
{

class ConnectorReport : public frontend::ConnectorReport
{
public:
    void thread_start() override;
    void thread_end() override;
    void starting_threads(int count) override;
    void stopping_threads(int count) override;

    void creating_session_for(int socket_handle) override;
    void creating_socket_pair(int server_handle, int client_handle) override;

    void listening_on(std::string const& endpoint) override;

    void error(std::exception const& error) override;
private:
    ServerTracepointProvider tp_provider;
};

}
}
}

#endif // MIR_REPORT_LTTNG_CONNECTOR_REPORT_H_
