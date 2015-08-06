/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/frontend/published_socket_connector.h"
#include "src/server/report/null/connector_report.h"
#include "mir/test/current_thread_name.h"
#include "mir/test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mt = mir::test;

namespace
{

struct StubConnectorReport : mir::report::null::ConnectorReport
{
    void thread_start()
    {
        thread_name = mt::current_thread_name();
    }

    std::string thread_name;
};

}

TEST(BasicConnector, names_ipc_threads)
{
    using namespace testing;

    StubConnectorReport report;
    int const num_threads = 1;
    mir::frontend::BasicConnector connector{
        {}, num_threads, mt::fake_shared(report)};

    connector.start();
    connector.stop();

    EXPECT_THAT(report.thread_name, Eq("Mir/IPC"));
}
