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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_DEVICE_SETTINGS_H_
#define MIR_INPUT_DEVICE_SETTINGS_H_

#include <boost/variant.hpp> // Sum type not yet in c++ stdlib
#include <glm/mat3x3.hpp> // Sum type not yet in c++ stdlib
#include <string>
#include <functional>

namespace mir
{
namespace input
{

class DeviceSettings
{
public:
    DeviceSettings() = default;
    virtual ~DeviceSettings() = default;

    struct NotApplicable {};

    /*!
     * Sum-type for possible device setting values.
     * NotApplicable is used to indicate unconfigureable settings.
     */
    using Value = boost::variant<NotApplicable, int, double, bool, std::string, glm::mat3x3>;

    enum Setting
    {
        /**
         * \brief Configure Left handed and right handed mode by selecting primary button (int)
         *   - 0: first button is primary (right handed)
         *   - 2: second button is primary (left handed)
         */
        primary_button = 1,
        /**
         * \brief Scale cursor accelaration. (double)
         *   - 0: default acceleration
         *   - [-1, 0[: reduced acceleration
         *   - ]0, 1]: increased acceleration
         */
        cursor_speed = 2,
        /**
         * \brief Scale scroll ticks (double)
         * Use negative values to invert scroll direction
         */
        horizontal_scroll_speed = 3,
        /**
         * \brief Scale scroll ticks (double)
         * Use negative values to invert scroll direction
         */
        vertical_scroll_speed = 4,
        /**
         * \brief Control natural scrolling (bool)
         * When enabled - finger movement on touchpad matches movement of the
         * document on screen.
         */
        natrual_scrolling = 5,
        /**
         * \brief Disable input device while typing on keyboard (bool)
         */
        disable_while_typing = 5,
        /**
         * \brief Disable (touchpad) when external mouse is plugged in (bool)
         */
        disable_with_mouse = 6,
        /**
         * Use a tap gesture to trigger primary button (bool)
         */
        tap_to_click = 7,
        /**
         * Use software button areas to generate button events(bool)
         */
        area_to_click = 8,
        middle_mouse_button_emulation = 9,
        /**
         * Scroll mode of touchpad (int):
         *  - 0: none
         *  - 1: Enables bidirectional scrolling with simultaneous movement of fingers (bool)
         *  - 2: Enables edge scrolling
         *  - 4: Button down and movement along scroll axis
         */
        scroll_mode= 10,
        /**
         * Configure the button to use for button scrolling (int)
         */
        scroll_button = 11,
        /**
         * 2D absolute device coordinate calibration matrix (glm::mat3x3)
         */
        coordinate_calibration = 12,
    };
    virtual Value get(Setting setting) const = 0;
    virtual void set(Setting setting, Value const& value) = 0;
    virtual void for_each_setting(
        std::function<void(Setting setting, Value const& value)> const& element_calback) const = 0;

private:
    DeviceSettings(DeviceSettings const&) = delete;
    DeviceSettings& operator=(DeviceSettings const&) = delete;
};

}
}

#endif
