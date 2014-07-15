/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "default_display_configuration_policy.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/pixel_format_utils.h"

#include <unordered_map>
#include <algorithm>

namespace mg = mir::graphics;
namespace geom = mir::geometry;

namespace
{
size_t select_mode_index(size_t mode_index, std::vector<mg::DisplayConfigurationMode> const & modes)
{
    if (modes.empty())
        return std::numeric_limits<size_t>::max();

    if (mode_index >= modes.size())
        return 0;

    return mode_index;
}

MirPixelFormat select_opaque_format(MirPixelFormat format, std::vector<MirPixelFormat> const& formats)
{
    auto const format_in_formats = formats.end() != std::find(formats.begin(), formats.end(), format);

    if (!mg::contains_alpha(format) && format_in_formats)
        return format;

    // format is either unavailable or transparent
    auto const first_opaque = std::find_if_not(formats.begin(), formats.end(), mg::contains_alpha);

    if (first_opaque != formats.end())
        return *first_opaque;

    // only tranparent options - allow choice if available
    if (format_in_formats)
        return format;

    if (formats.size())
        return formats.at(0);

    return mir_pixel_format_invalid;
}

}

void mg::DefaultDisplayConfigurationPolicy::apply_to(DisplayConfiguration& conf)
{
    static MirPowerMode const default_power_state = mir_power_mode_on;
    std::unordered_map<DisplayConfigurationCardId, size_t> available_outputs_for_card;

    conf.for_each_card(
        [&](DisplayConfigurationCard const& card)
        {
            available_outputs_for_card[card.id] = card.max_simultaneous_outputs;
        });

    conf.for_each_output(
        [&](UserDisplayConfigurationOutput& conf_output)
        {
            if (!conf_output.connected || conf_output.modes.empty() ||
                available_outputs_for_card[conf_output.card_id] == 0)
            {
                conf_output.used = false;
                conf_output.power_mode = default_power_state;
                return;
            }

            size_t preferred_mode_index{select_mode_index(conf_output.preferred_mode_index, conf_output.modes)};
            MirPixelFormat format{select_opaque_format(conf_output.current_format, conf_output.pixel_formats)};

            conf_output.used = true;
            conf_output.top_left = geom::Point{0, 0};
            conf_output.current_mode_index = preferred_mode_index;
            conf_output.current_format = format;
            conf_output.power_mode = default_power_state;
            conf_output.orientation = mir_orientation_normal;

            --available_outputs_for_card[conf_output.card_id];
        });
}

