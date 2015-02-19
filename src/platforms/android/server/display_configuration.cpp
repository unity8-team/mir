/*
 * Copyright © 2013 Canonical Ltd.
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
 */

#include "display_configuration.h"
#include <boost/throw_exception.hpp>

namespace mg = mir::graphics;
namespace mga = mg::android;
namespace geom = mir::geometry;

namespace
{
int const primary_id{0};
int const external_id{1};
geom::Point const origin{0,0};
size_t const preferred_format_index{0};
size_t const preferred_mode_index{0};

geom::Size bound_size(geom::Size primary, geom::Size external)
{
    geom::Size transposed_external{external.height.as_int(), external.width.as_int()};
    return geom::Size {
        std::min(primary.width, transposed_external.width),
        std::min(primary.height, transposed_external.height)
    };
}

mg::DisplayConfigurationOutput external_output(
    mga::DisplayAttribs const& external_attribs,
    MirPowerMode external_mode, geom::Size forced_size)
{
    std::vector<mg::DisplayConfigurationMode> external_modes;
    if (external_attribs.connected)
    {
        external_modes.emplace_back(
            mg::DisplayConfigurationMode{forced_size, external_attribs.vrefresh_hz});
    }

    bool used{false};
    return {
        mg::DisplayConfigurationOutputId{external_id},
        mg::DisplayConfigurationCardId{0},
        mg::DisplayConfigurationOutputType::displayport,
        {external_attribs.display_format},
        external_modes,
        preferred_mode_index,
        external_attribs.mm_size,
        external_attribs.connected,
        used,
        origin,
        preferred_format_index,
        external_attribs.display_format,
        external_mode,
        mir_orientation_left
    };
}
}

mga::DisplayConfiguration::DisplayConfiguration(
    mga::DisplayAttribs const& primary_attribs,
    MirPowerMode primary_mode,
    mga::DisplayAttribs const& external_attribs,
    MirPowerMode external_mode) :
    forced_size(bound_size(primary_attribs.pixel_size, external_attribs.pixel_size)),
    configurations{{
        mg::DisplayConfigurationOutput{
            mg::DisplayConfigurationOutputId{primary_id},
            mg::DisplayConfigurationCardId{0},
            mg::DisplayConfigurationOutputType::lvds,
            {primary_attribs.display_format},
            {mg::DisplayConfigurationMode{forced_size, primary_attribs.vrefresh_hz}},
            preferred_mode_index,
            primary_attribs.mm_size,
            primary_attribs.connected,
            true,
            origin,
            preferred_format_index,
            primary_attribs.display_format,
            primary_mode,
            mir_orientation_normal
        }, 
        external_output(external_attribs, external_mode, forced_size)
    }},
    card{mg::DisplayConfigurationCardId{0}, 1}
{
}

mga::DisplayConfiguration::DisplayConfiguration(DisplayConfiguration const& other) :
    mg::DisplayConfiguration(),
    configurations(other.configurations),
    card(other.card)
{
}

mga::DisplayConfiguration& mga::DisplayConfiguration::operator=(DisplayConfiguration const& other)
{
    if (&other != this)
    {
        configurations = other.configurations;
        card = other.card;
    }
    return *this;
}

void mga::DisplayConfiguration::for_each_card(std::function<void(mg::DisplayConfigurationCard const&)> f) const
{
    f(card);
}

void mga::DisplayConfiguration::for_each_output(std::function<void(mg::DisplayConfigurationOutput const&)> f) const
{
    for (auto const& configuration : configurations)
        f(configuration);
}

void mga::DisplayConfiguration::for_each_output(std::function<void(mg::UserDisplayConfigurationOutput&)> f)
{
    for (auto& configuration : configurations)
    {
        mg::UserDisplayConfigurationOutput user(configuration);
        f(user);
    }
}

mg::DisplayConfigurationOutput& mga::DisplayConfiguration::primary()
{
    return configurations[primary_id];
}

mg::DisplayConfigurationOutput& mga::DisplayConfiguration::external()
{
    return configurations[external_id];
}

mg::DisplayConfigurationOutput& mga::DisplayConfiguration::operator[](mg::DisplayConfigurationOutputId const& disp_id)
{
    auto id = disp_id.as_value();
    if (id != primary_id && id != external_id)
        BOOST_THROW_EXCEPTION(std::invalid_argument("invalid display id"));
    return configurations[id];
}
