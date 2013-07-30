/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "kms_display_configuration.h"
#include "drm_mode_resources.h"

#include <cmath>
#include <limits>

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <algorithm>

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;

namespace
{

bool kms_modes_are_equal(drmModeModeInfo const& info1, drmModeModeInfo const& info2)
{
    return (info1.clock == info2.clock &&
            info1.hdisplay == info2.hdisplay &&
            info1.hsync_start == info2.hsync_start &&
            info1.hsync_end == info2.hsync_end &&
            info1.htotal == info2.htotal &&
            info1.hskew == info2.hskew &&
            info1.vdisplay == info2.vdisplay &&
            info1.vsync_start == info2.vsync_start &&
            info1.vsync_end == info2.vsync_end &&
            info1.vtotal == info2.vtotal);
}

double calculate_vrefresh_hz(drmModeModeInfo const& mode)
{
    if (mode.htotal == 0 || mode.vtotal == 0)
        return 0.0;

    /* mode.clock is in KHz */
    double vrefresh_hz = mode.clock * 1000.0 / (mode.htotal * mode.vtotal);

    /* Round to first decimal */
    return round(vrefresh_hz * 10.0) / 10.0;
}

}

mgg::KMSDisplayConfiguration::KMSDisplayConfiguration(int drm_fd)
    : drm_fd{drm_fd}
{
    update();
}

mgg::KMSDisplayConfiguration::KMSDisplayConfiguration(
    KMSDisplayConfiguration const& conf)
    : DisplayConfiguration(), drm_fd{conf.drm_fd}, outputs{conf.outputs}
{
}

mgg::KMSDisplayConfiguration& mgg::KMSDisplayConfiguration::operator=(
    KMSDisplayConfiguration const& conf)
{
    if (&conf != this)
    {
        drm_fd = conf.drm_fd;
        outputs = conf.outputs;
    }

    return *this;
}

void mgg::KMSDisplayConfiguration::for_each_card(
    std::function<void(DisplayConfigurationCard const&)> f) const
{
    DisplayConfigurationCard const card{DisplayConfigurationCardId{0}};
    f(card);
}

void mgg::KMSDisplayConfiguration::for_each_output(
    std::function<void(DisplayConfigurationOutput const&)> f) const
{
    for (auto const& output : outputs)
        f(output);
}

void mgg::KMSDisplayConfiguration::configure_output(
    DisplayConfigurationOutputId id, bool used,
    geometry::Point top_left, size_t mode_index)
{
    auto iter = find_output_with_id(id);

    if (iter != outputs.end())
    {
        auto& output = *iter;

        if (used && mode_index >= output.modes.size())
            BOOST_THROW_EXCEPTION(std::runtime_error("Invalid mode_index for used output"));

        output.used = used;
        output.top_left = top_left;
        output.current_mode_index = mode_index;
    }
    else
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Trying to configure invalid output"));
    }
}

uint32_t mgg::KMSDisplayConfiguration::get_kms_connector_id(DisplayConfigurationOutputId id) const
{
    for (auto const& output : outputs)
    {
        if (output.id == id)
            return id.as_value();
    }

    BOOST_THROW_EXCEPTION(std::runtime_error("Failed to find DisplayConfigurationOutput with provided id"));
}

void mgg::KMSDisplayConfiguration::update()
{
    DRMModeResources resources{drm_fd};

    resources.for_each_connector([&](DRMModeConnectorUPtr connector)
    {
        add_or_update_output(resources, *connector);
    });
}

void mgg::KMSDisplayConfiguration::add_or_update_output(
    DRMModeResources const& resources,
    drmModeConnector const& connector)
{
    DisplayConfigurationOutputId id{static_cast<int>(connector.connector_id)};
    DisplayConfigurationCardId card_id{0};
    geom::Size physical_size{connector.mmWidth, connector.mmHeight};
    bool connected{connector.connection == DRM_MODE_CONNECTED};
    size_t current_mode_index{std::numeric_limits<size_t>::max()};
    std::vector<DisplayConfigurationMode> modes;
    drmModeModeInfo current_mode_info = drmModeModeInfo();

    /* Get information about the current mode */
    auto encoder = resources.encoder(connector.encoder_id);
    if (encoder)
    {
        auto crtc = resources.crtc(encoder->crtc_id);
        if (crtc)
            current_mode_info = crtc->mode;
    }

    /* Add all the available modes and find the current one */
    for (int m = 0; m < connector.count_modes; m++)
    {
        drmModeModeInfo& mode_info = connector.modes[m];

        geom::Size size{mode_info.hdisplay, mode_info.vdisplay};

        double vrefresh_hz = calculate_vrefresh_hz(mode_info);

        modes.push_back({size, vrefresh_hz});

        if (kms_modes_are_equal(mode_info, current_mode_info))
            current_mode_index = m;
    }

    /* Add or update the output */
    auto iter = find_output_with_id(id);

    if (iter == outputs.end())
    {
        outputs.push_back({id, card_id, modes, physical_size,
                           connected, false, geom::Point(), current_mode_index});
    }
    else
    {
        auto& output = *iter;

        output.modes = modes;
        output.physical_size_mm = physical_size;
        output.connected = connected;
        output.current_mode_index = current_mode_index;
    }
}

std::vector<mg::DisplayConfigurationOutput>::iterator
mgg::KMSDisplayConfiguration::find_output_with_id(DisplayConfigurationOutputId id)
{
    return std::find_if(outputs.begin(), outputs.end(),
                        [id](DisplayConfigurationOutput const& output)
                        {
                            return output.id == id;
                        });
}
