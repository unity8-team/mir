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

#ifndef MIR_INPUT_DEFAULT_INPUT_DEVICE_HUB_H_
#define MIR_INPUT_DEFAULT_INPUT_DEVICE_HUB_H_

#include "main_loop_wrapper.h"

#include "mir/input/input_device_registry.h"
#include "mir/input/input_event_handler_register.h"
#include "mir/input/input_sink.h"
#include "mir/input/input_device_hub.h"
#include "mir/input/input_device_info.h"
#include "mir/input/input_device_observer.h"

#include "mir_toolkit/event.h"

#include <vector>
#include <memory>

namespace mir
{
class MainLoop;
class ServerActionQueue;
namespace input
{
class InputDispatcher;
class InputSink;
class InputDeviceObserver;

class DefaultInputDeviceHub : public InputDeviceRegistry, public InputDeviceHub
{
public:
    DefaultInputDeviceHub(std::shared_ptr<InputDispatcher> const& input_dispatcher, std::shared_ptr<MainLoop> const& input_loop, std::shared_ptr<ServerActionQueue> const& observer_queue);

    // InputDeviceRegistry - calls from mi::Platform
    void add_device(std::shared_ptr<InputDevice> const& device) override;
    void remove_device(std::shared_ptr<InputDevice> const& device) override;

    // InputDeviceHub - calls from server components / shell
    void add_observer(std::shared_ptr<InputDeviceObserver> const&) override;
    void remove_observer(std::weak_ptr<InputDeviceObserver> const&) override;

private:
    void add_device_info(InputDeviceInfo const& info);
    void remove_device_info(InputDeviceInfo const& info);
    std::shared_ptr<InputDispatcher> const input_dispatcher;
    MainLoopWrapper input_handler_register;
    std::shared_ptr<ServerActionQueue> const observer_queue;

    struct RegisteredDevice : public InputSink
    {
    public:
        RegisteredDevice(std::shared_ptr<InputDevice> const& dev, std::shared_ptr<InputDispatcher> const& dispatcher);
        void handle_input(MirEvent& event) override;
        bool device_matches(std::shared_ptr<InputDevice> const& dev) const;
        void start(InputEventHandlerRegister& registry);
        void stop(InputEventHandlerRegister& registry);
        InputDeviceInfo get_device_info();
    private:
        static int32_t create_new_device_id();
        int32_t device_id;
        std::shared_ptr<InputDevice> device;
        std::shared_ptr<InputDispatcher> dispatcher;
    };

    // DeviceInfo ~ and device id? ~
    std::vector<InputDeviceInfo> infos;
    std::vector<std::unique_ptr<RegisteredDevice>> devices;
    // weak?
    std::vector<std::shared_ptr<InputDeviceObserver>> observers;
};

}
}

#endif