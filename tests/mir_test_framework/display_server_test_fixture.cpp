/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/testing_client_configuration.h"

namespace mc = mir::compositor;
namespace mtf = mir_test_framework;

void BespokeDisplayServerTestFixture::launch_server_process(TestingServerConfiguration& functor)
{
    test_options = functor.the_options();
    process_manager.launch_server_process(functor);
}

pid_t BespokeDisplayServerTestFixture::launch_client_process(TestingClientConfiguration& config)
{
    return process_manager.launch_client_process(config, *test_options);
}

bool BespokeDisplayServerTestFixture::shutdown_server_process()
{
    // TODO fix problem and remove this frig.
    // problem: sometimes the server exits normally with status
    // EXIT_SUCCESS but the test process sees TerminationReason::unknown
    auto const& result = process_manager.shutdown_server_process();
    return result.succeeded()
        || result.reason == TerminationReason::unknown;
}

bool BespokeDisplayServerTestFixture::kill_server_process()
{
    auto const& result = process_manager.kill_server_process();
    return result.reason == TerminationReason::child_terminated_by_signal &&
           result.signal == SIGKILL;
}

mtf::Result BespokeDisplayServerTestFixture::wait_for_shutdown_server_process()
{
    return process_manager.wait_for_shutdown_server_process();
}

std::vector<mtf::Result> BespokeDisplayServerTestFixture::wait_for_shutdown_client_processes()
{
    return process_manager.wait_for_shutdown_client_processes();
}

void BespokeDisplayServerTestFixture::terminate_client_processes()
{
    process_manager.terminate_client_processes();
}

void BespokeDisplayServerTestFixture::run_in_test_process(
    std::function<void()> const& run_code)
{
    process_manager.run_in_test_process(run_code);
}

void BespokeDisplayServerTestFixture::SetUp()
{
}

void BespokeDisplayServerTestFixture::TearDown()
{
    process_manager.tear_down_all();
}

BespokeDisplayServerTestFixture::BespokeDisplayServerTestFixture() :
    process_manager()
{
}

BespokeDisplayServerTestFixture::~BespokeDisplayServerTestFixture() {}
