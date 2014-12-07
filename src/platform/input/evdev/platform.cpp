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
#include "evdev_input_device_factory.h"
#include "android_device_provider.h"
#include "libinput_device_provider.h"
#include "mir/udev/wrapper.h"

#include "mir/input/input_event_handler_register.h"
#include "mir/input/input_device_registry.h"
#include "mir/input/input_device.h"

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
    this->monitor->filter_by_subsystem("input");
    this->monitor->enable();
}

void mie::Platform::start_monitor_devices(mi::InputEventHandlerRegister& execution, std::shared_ptr<InputDeviceRegistry> const& input_device_registry)
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
                    if (!dev.devnode())
                        return;
                    if (event == mu::Monitor::ADDED)
                        device_added(dev);
                    if (event == mu::Monitor::REMOVED)
                        device_removed(dev);
                    if (event == mu::Monitor::CHANGED)
                        device_changed(dev);
                }
                );
        });

    execution.register_handler([this](){ scan_for_devices();});
}

void mie::Platform::scan_for_devices()
{
    mu::Enumerator input_enumerator{udev_context};
    input_enumerator.match_subsystem("input");
    input_enumerator.scan_devices();

    for (auto& device : input_enumerator)
    {
        if (device.devnode() != nullptr)
            device_added(device);
    }
}

void mie::Platform::device_added(mu::Device const& dev)
{
    if (end(devices) != find_device(dev.devnode()))
        return;

    std::shared_ptr<mi::InputDevice> input_dev;

    try
    {
        input_dev = input_device_factory->create_device(dev.devnode());
    } catch(...)
    {
        // report creation failure
    }

    input_device_registry->add_device(input_dev);
    devices.emplace_back(dev.devnode(), input_dev);
}

void mie::Platform::device_removed(mu::Device const& dev)
{
    auto known_device_pos = find_device(dev.devnode());

    if (known_device_pos == end(devices))
        return;

    input_device_registry->remove_device(known_device_pos->second);
    devices.erase(known_device_pos);
}


auto mie::Platform::find_device(char const* devnode) -> decltype(devices)::iterator
{
    return std::find_if(
        begin(devices),
        end(devices),
        [devnode](decltype(devices)::value_type const& item)
        {
            return devnode == item.first;
        }
        );
}

void mie::Platform::device_changed(mu::Device const& /*dev*/)
{
}

void mie::Platform::stop_monitor_devices(mi::InputEventHandlerRegister& execution)
{
    execution.unregister_fd_handler(this);
}

std::unique_ptr<mie::Platform> mie::create_evdev_input_platform(
    std::shared_ptr<mi::InputReport> const& report)
{
    std::unique_ptr<mu::Context> ctx{new mu::Context};
    std::unique_ptr<mu::Monitor> monitor{new mu::Monitor(*ctx.get())};
    std::initializer_list<std::shared_ptr<mie::InputDeviceProvider>> providers =
        {std::make_shared<mie::AndroidDeviceProvider>(), std::make_shared<mie::LibInputDeviceProvider>()};
    return std::unique_ptr<mie::Platform>(
        new mie::Platform(
            report,
            std::move(ctx),
            std::move(monitor),
            std::make_shared<mie::EvdevInputDeviceFactory>(providers))
        );
}

extern "C" std::unique_ptr<mi::Platform> create_input_platform(
    std::shared_ptr<mo::Option> const& /*options*/,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& /*emergency_cleanup_registry*/,
    std::shared_ptr<mi::InputReport> const& report)
{
    return std::move( mie::create_evdev_input_platform(report) );
}


extern "C" void add_input_platform_options(
    boost::program_options::options_description& /*config*/)
{
    // no options to add yet
}

extern "C" mi::PlatformPriority probe_input_platform(
    std::shared_ptr<mo::Option> const& options)
{
    if (options->is_set(host_socket_opt))
    {
        return mi::PlatformPriority::unsupported;
    }
    return mi::PlatformPriority::supported;
}
