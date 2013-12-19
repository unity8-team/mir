/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include <stdexcept>
#include <algorithm>
#include <boost/exception/all.hpp>
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/pixel_format_utils.h"
#include "translucent_outputs.h"

namespace mir
{
namespace examples
{

TranslucentOutputs::TranslucentOutputs(std::shared_ptr<DisplayConfigurationPolicy> const& base_policy)
    : base_policy(base_policy)
{}

void TranslucentOutputs::apply_to(graphics::DisplayConfiguration & conf)
{
    base_policy->apply_to(conf);
    conf.for_each_output(
        [&](graphics::DisplayConfigurationOutput const& conf_output)
        {
            if (!conf_output.connected || !conf_output.used) return;

            auto const& pos = find_if(conf_output.pixel_formats.begin(),
                                      conf_output.pixel_formats.end(),
                                      &graphics::contains_alpha);

            // do not touch anything if no alpha available
            if (pos == conf_output.pixel_formats.end())
                return;

            conf.configure_output(conf_output.id, true, conf_output.top_left,
                                  conf_output.current_mode_index,
                                  std::distance(conf_output.pixel_formats.begin(), pos ),
                                  conf_output.power_mode
                                 );
        });
}
}
}

