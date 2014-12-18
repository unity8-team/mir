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

#ifndef MIR_INPUT_EVDEV_LIBINPUT_DEVICE_H_
#define MIR_INPUT_EVDEV_LIBINPUT_DEVICE_H_

#include "mir/input/input_device.h"
#include "mir_toolkit/event.h"

struct libinput_device;
struct libinput_event;
struct libinput_event_keyboard;
struct libinput_event_touch;

namespace mir
{
namespace input
{
namespace evdev
{
class LibInputWrapper;

class LibInputDevice : public input::InputDevice
{
public:
    LibInputDevice(std::shared_ptr<LibInputWrapper> const& lib, char const* path);
    ~LibInputDevice();
    void start(InputEventHandlerRegister& registry, EventSink& sink) override;
    void stop(InputEventHandlerRegister& registry) override;
    void set_cursor_filter(std::shared_ptr<CursorFilter> filter) override;
    libinput_device* device() const;
    void process_event(libinput_event* event);
private:
    std::string path;
    std::unique_ptr<::libinput_device,libinput_device*(*)(::libinput_device*)> dev;
    std::shared_ptr<LibInputWrapper> const lib;
    EventSink *sink;

    struct PointerState;
    struct KeyboardState;
    std::unique_ptr<PointerState> pointer;
    std::unique_ptr<KeyboardState> keyboard;
    PointerState& get_pointer();
    KeyboardState& get_keyboard();
};
}
}
}

#endif
