/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/input/platform.h"
#include <vector>

namespace mir
{
namespace udev
{
class Device;
class Monitor;
class Context;
}
namespace input
{
class InputDeviceInfo;
namespace evdev
{
class InputDeviceFactory;

class Platform : public input::Platform
{
public:
    Platform(std::shared_ptr<InputReport> const& report,
             std::unique_ptr<udev::Context> udev_context,
             std::unique_ptr<udev::Monitor> monitor,
             std::shared_ptr<InputDeviceFactory> const& factory);
    void start_monitor_devices(Multiplexer& loop, std::shared_ptr<InputDeviceRegistry> const& input_device_registry) override;
    void stop_monitor_devices(Multiplexer& loop) override;

private:
    void scan_for_devices();
    void device_added(udev::Device const& dev);
    void device_removed(udev::Device const& dev);
    void device_changed(udev::Device const& dev);
    std::shared_ptr<InputReport> const report;
    std::shared_ptr<udev::Context> udev_context;
    std::unique_ptr<udev::Monitor> monitor;
    std::shared_ptr<InputDeviceRegistry> input_device_registry;
    std::shared_ptr<InputDeviceFactory> input_device_factory;

    std::vector<std::pair<std::string,std::shared_ptr<InputDevice>>> devices;
};


std::unique_ptr<Platform> create_evdev_input_platform(std::shared_ptr<InputReport> const& report);

}
}
}
