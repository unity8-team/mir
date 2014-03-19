/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by:
 * Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_TESTING_UDEV_ENVIRONMENT
#define MIR_TESTING_UDEV_ENVIRONMENT

#include <string>
#include <initializer_list>
#include <umockdev.h>
#include <libudev.h>

namespace mir
{
namespace mir_test_framework
{
class UdevEnvironment
{
public:
    UdevEnvironment();
    ~UdevEnvironment() noexcept;

    std::string add_device(char const* subsystem,
                           char const* name,
                           char const* parent,
                           std::initializer_list<char const*> attributes,
                           std::initializer_list<char const*> properties);
    void remove_device(std::string const& device_path);
    void emit_device_changed(std::string const& device_path);

    /**
     * Add a device from the set of standard device traces
     *
     * Looks for a <tt>name</tt>.umockdev file, and adds a UMockDev device
     * from that description.
     *
     * If <tt>name</tt>.ioctl exists, it loads that ioctl script for the device
     *
     * @param name The unadorned filename of the device traces to add.
     */
    void add_standard_device(std::string const& name);

    UMockdevTestbed *testbed;
    std::string const recordings_path;
};

}
}

#endif //MIR_TESTING_UDEV_ENVIRONMENT
