/*
 * Copyright © 2015 Canonical Ltd.
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

#include "libinput_ptr.h"
#include "libinput_device_ptr.h"

#include "mir/input/event_builder.h"
#include "mir/input/input_device.h"
#include "mir/input/input_device_info.h"
#include "mir/geometry/point.h"

#include <vector>
#include <map>

struct libinput_event;
struct libinput_event_keyboard;
struct libinput_event_touch;
struct libinput_event_pointer;
struct libinput_device_group;

namespace mir
{
namespace input
{
class InputReport;
namespace evdev
{
struct PointerState;
struct KeyboardState;

class LibInputDevice : public input::InputDevice
{
public:
    LibInputDevice(std::shared_ptr<InputReport> const& report, char const* path, LibInputDevicePtr dev);
    ~LibInputDevice();
    void start(InputSink* sink, EventBuilder* builder) override;
    void stop() override;
    InputDeviceInfo get_device_info() override;
    optional_value<PointerSettings> get_pointer_settings() const override;
    void apply_settings(PointerSettings const&) override;
    optional_value<TouchpadSettings> get_touchpad_settings() const override;
    void apply_settings(TouchpadSettings const&) override;

    void process_event(libinput_event* event);
    ::libinput_device* device() const;
    ::libinput_device_group* group();
    void add_device_of_group(char const* path, LibInputDevicePtr ptr);
    bool is_in_group(char const* path);
private:
    EventUPtr convert_event(libinput_event_keyboard* keyboard);
    EventUPtr convert_button_event(libinput_event_pointer* pointer);
    EventUPtr convert_motion_event(libinput_event_pointer* pointer);
    EventUPtr convert_absolute_motion_event(libinput_event_pointer* pointer);
    EventUPtr convert_axis_event(libinput_event_pointer* pointer);
    EventUPtr convert_touch_frame(libinput_event_touch* touch);
    void handle_touch_down(libinput_event_touch* touch);
    void handle_touch_up(libinput_event_touch* touch);
    void handle_touch_motion(libinput_event_touch* touch);
    void update_device_info();

    std::shared_ptr<InputReport> report;
    std::shared_ptr<::libinput> lib;
    std::vector<std::string> paths;
    std::vector<LibInputDevicePtr> devices;
    std::shared_ptr<dispatch::Dispatchable> dispatchable_fd;

    InputSink* sink{nullptr};
    EventBuilder* builder{nullptr};

    InputDeviceInfo info;
    mir::geometry::Point pointer_pos;
    MirPointerButtons button_state;
    double vertical_scroll_scale{1.0};
    double horizontal_scroll_scale{1.0};

    struct ContactData
    {
        ContactData() {}
        MirTouchAction action{mir_touch_action_change};
        float x{0}, y{0}, major{0}, minor{0}, pressure{0};
    };
    std::map<MirTouchId,ContactData> last_seen_properties;

    void update_contact_data(ContactData &data, MirTouchAction action, libinput_event_touch* touch);
};
}
}
}

#endif
