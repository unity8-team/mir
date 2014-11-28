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

#include "platform.h"
#include "input_device_factory.h"
#include "mir/udev/wrapper.h"

#include "mir/input/multiplexer.h"

namespace mi = mir::input;
namespace mo = mir::options;
namespace mu = mir::udev;
namespace mie = mi::evdev;

namespace
{
char const* const host_socket_opt = "host-socket";
}

mie::Platform::Platform(std::shared_ptr<InputReport> const& report,
                        std::unique_ptr<udev::Context> udev_context,
                        std::unique_ptr<udev::Monitor> monitor,
                        std::shared_ptr<mie::InputDeviceFactory> const& factory) :
    report(report),
    udev_context(std::move(udev_context)),
    monitor(std::move(monitor)),
    input_device_factory(factory)
{
}

void mie::Platform::start_monitor_devices(mi::Multiplexer& execution, std::shared_ptr<InputDeviceRegistry> const& input_device_registry)
{
    this->input_device_registry = input_device_registry;
    execution.register_fd_handler(
        {monitor->fd()},
        this,
        [this](int /*fd*/)
        {
           monitor->process_events(
               [this](mu::Monitor::EventType event, mu::Device const& dev)
               {
                   if (evet == mu::Monitor::ADDED)
                       device_added(dev);
                   if (evet == mu::Monitor::REMOVED)
                       device_removed(dev);
                   if (evet == mu::Monitor::CHANGED)
                       device_changed(dev);
               }
               );
        });
}

void mie::Platform::device_added(udev::Device const& dev)
{
}

void mie::Platform::device_removed(udev::Device const& dev)
{
}

void mie::Platform::device_changed(udev::Device const& dev)
{
}

void mie::Platform::stop_monitor_devices(mi::Multiplexer& execution)
{
    execution.unregister_fd_handler(this);
}

extern "C" std::shared_ptr<mi::Platform> create_platform(
    std::shared_ptr<mo::Option> const& /*options*/,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& /*emergency_cleanup_registry*/,
    std::shared_ptr<mi::InputReport> const& report)
{
    mir::udev::Context * ctx = new mir::udev::Context;
    return std::make_shared<mie::Platform>(
        report,
        ctx,
        new mir::udev::Monitor(*ctx));
}


extern "C" void add_platform_options(
    boost::program_options::options_description& /*config*/)
{
    // no options to add yet
}

extern "C" mi::PlatformPriority probe_platform(
    std::shared_ptr<mo::Option> const& options)
{
    if (options->is_set(host_socket_opt))
    {
        return mi::PlatformPriority::unsupported;
    }
}
