/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "example_display_configuration_policy.h"

#include "mir/graphics/display_configuration.h"
#include "mir/server.h"
#include "mir/options/option.h"

#include <algorithm>
#include <unordered_map>
#include <stdexcept>

namespace geom = mir::geometry;
namespace me = mir::examples;
namespace mg = mir::graphics;

char const* const me::display_config_opt = "display-config";
char const* const me::display_config_descr = "Display configuration [{clone,sidebyside,single}]";

char const* const me::clone_opt_val = "clone";
char const* const me::sidebyside_opt_val = "sidebyside";
char const* const me::single_opt_val = "single";

char const* const me::display_alpha_opt = "translucent";
char const* const me::display_alpha_descr = "Select a display mode with alpha[{on,off}]";

char const* const me::display_alpha_off = "off";
char const* const me::display_alpha_on = "on";

void me::SideBySideDisplayConfigurationPolicy::apply_to(graphics::DisplayConfiguration& conf)
{
    size_t const preferred_mode_index{0};
    int max_x = 0;
    std::unordered_map<mg::DisplayConfigurationCardId, size_t> available_outputs_for_card;

    conf.for_each_card(
        [&](mg::DisplayConfigurationCard const& card)
        {
            available_outputs_for_card[card.id] = card.max_simultaneous_outputs;
        });

    conf.for_each_output(
        [&](mg::UserDisplayConfigurationOutput& conf_output)
        {
            if (conf_output.connected && conf_output.modes.size() > 0 &&
                available_outputs_for_card[conf_output.card_id] > 0)
            {
                conf_output.used = true;
                conf_output.top_left = geom::Point{max_x, 0};
                conf_output.current_mode_index = preferred_mode_index;
                conf_output.power_mode = mir_power_mode_on;
                conf_output.orientation = mir_orientation_normal;
                max_x += conf_output.modes[preferred_mode_index].size.width.as_int();
                --available_outputs_for_card[conf_output.card_id];
            }
            else
            {
                conf_output.used = false;
                conf_output.power_mode = mir_power_mode_off;
            }
        });
}


void me::SingleDisplayConfigurationPolicy::apply_to(graphics::DisplayConfiguration& conf)
{
    size_t const preferred_mode_index{0};
    bool done{false};

    conf.for_each_output(
        [&](mg::UserDisplayConfigurationOutput& conf_output)
        {
            if (!done && conf_output.connected && conf_output.modes.size() > 0)
            {
                conf_output.used = true;
                conf_output.top_left = geom::Point{0, 0};
                conf_output.current_mode_index = preferred_mode_index;
                conf_output.power_mode = mir_power_mode_on;
                done = true;
            }
            else
            {
                conf_output.used = false;
                conf_output.power_mode = mir_power_mode_off;
            }
        });
}

namespace
{
bool contains_alpha(MirPixelFormat format)
{
    return (format == mir_pixel_format_abgr_8888 ||
            format == mir_pixel_format_argb_8888);
}
}

me::PixelFormatSelector::PixelFormatSelector(std::shared_ptr<DisplayConfigurationPolicy> const& base_policy,
                                         bool with_alpha)
    : base_policy{base_policy},
    with_alpha{with_alpha}
{}

void me::PixelFormatSelector::apply_to(graphics::DisplayConfiguration & conf)
{
    base_policy->apply_to(conf);
    conf.for_each_output(
        [&](graphics::UserDisplayConfigurationOutput& conf_output)
        {
            if (!conf_output.connected || !conf_output.used) return;

            auto const& pos = find_if(conf_output.pixel_formats.begin(),
                                      conf_output.pixel_formats.end(),
                                      [&](MirPixelFormat format) -> bool
                                          {
                                              return contains_alpha(format) == with_alpha;
                                          }
                                     );

            // keep the default settings if nothing was found
            if (pos == conf_output.pixel_formats.end())
                return;

            conf_output.current_format = *pos;
        });
}

void me::add_display_configuration_options_to(mir::Server& server)
{
    // Add choice of monitor configuration
    server.add_configuration_option(
        me::display_config_opt, me::display_config_descr,   me::clone_opt_val);
    server.add_configuration_option(
        me::display_alpha_opt,  me::display_alpha_descr,    me::display_alpha_off);

    server.wrap_display_configuration_policy(
        [&](std::shared_ptr<mg::DisplayConfigurationPolicy> const& wrapped)
        -> std::shared_ptr<mg::DisplayConfigurationPolicy>
        {
            auto const options = server.get_options();
            auto display_layout = options->get<std::string>(me::display_config_opt);
            auto with_alpha = options->get<std::string>(me::display_alpha_opt) == me::display_alpha_on;

            auto layout_selector = wrapped;

            if (display_layout == me::sidebyside_opt_val)
                layout_selector = std::make_shared<me::SideBySideDisplayConfigurationPolicy>();
            else if (display_layout == me::single_opt_val)
                layout_selector = std::make_shared<me::SingleDisplayConfigurationPolicy>();

            // Whatever the layout select a pixel format with requested alpha
            return std::make_shared<me::PixelFormatSelector>(layout_selector, with_alpha);
        });
}
