/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef MIR_TEST_DOUBLES_MOCK_LOGGER_H_
#define MIR_TEST_DOUBLES_MOCK_LOGGER_H_

#include "mir/logging/logger.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockLogger : public mir::logging::Logger
{
public:
    MOCK_METHOD3(log, void(mir::logging::Severity, const std::string&, const std::string&));
    MOCK_METHOD1(set_level, void(mir::logging::Severity));
    ~MockLogger() noexcept(true) {}
};

} // namespace doubles
} // namespace test
} // namespace mir

#endif /* MIR_TEST_DOUBLES_MOCK_LOGGER_H_ */
