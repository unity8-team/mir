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

#ifndef MIR_INPUT_EVDEV_LIBINPUT_WRAPPER_H_
#define MIR_INPUT_EVDEV_LIBINPUT_WRAPPER_H_

#include <memory>
#include <vector>

struct libinput;
struct libinput_device;

namespace mir
{
namespace input
{
class InputEventHandlerRegister;
class EventSink;
namespace evdev
{
class LibInputWrapper
{
public:
    LibInputWrapper();
    ~LibInputWrapper();

    void enable_input_processing(InputEventHandlerRegister& mplex, ::libinput_device *dev, EventSink& sink);
    void disable_input_processing(InputEventHandlerRegister& mplex, ::libinput_device *dev);

    typedef ::libinput_device*(*DeviceDeleter)(::libinput_device*);
    std::unique_ptr<::libinput_device,DeviceDeleter> add_device(std::string const& path);
private:
    void handle_devices();
    LibInputWrapper(LibInputWrapper const&) = delete;
    LibInputWrapper& operator=(LibInputWrapper const&) = delete;

    std::unique_ptr<::libinput,::libinput*(*)(::libinput*)> lib;
    std::vector<std::pair<::libinput_device*,EventSink*>> active_devices;
};
}
}
}

#endif
