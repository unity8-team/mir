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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "stub_input_platform.h"
#include "fake_input_device_impl.h"

namespace mtf = mir_test_framework;
namespace mo = mir::options;
namespace mi = mir::input;

extern "C" std::unique_ptr<mi::Platform> create_input_platform(
    std::shared_ptr<mo::Option> const& /*options*/,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& /*emergency_cleanup_registry*/,
    std::shared_ptr<mi::InputReport> const& /*report*/)
{
    return std::unique_ptr<mtf::StubInputPlatform>(new mtf::StubInputPlatform);
}


extern "C" void add_input_platform_options(
    boost::program_options::options_description& /*config*/)
{
    // no options to add yet
}

extern "C" mi::PlatformPriority probe_input_platform(
    mo::Option const& /*options*/)
{
    return mi::PlatformPriority::supported;
}

extern "C" std::unique_ptr<mtf::FakeInputDevice> add_fake_input_device(mi::InputDeviceInfo const& info)
{
    return std::unique_ptr<mtf::FakeInputDevice>(new mtf::FakeInputDeviceImpl(info));
}

