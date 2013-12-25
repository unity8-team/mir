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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/graphics/default_display_configuration_policy.h"
#include "mir/graphics/display_configuration.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace geom = mir::geometry;

namespace
{

class MockDisplayConfiguration : public mg::DisplayConfiguration
{
public:
    MockDisplayConfiguration(MockDisplayConfiguration && m)
        : max_simultaneous_outputs{m.max_simultaneous_outputs},
        outputs{std::move(m.outputs)}
    {
    }

    MockDisplayConfiguration(size_t max_simultaneous_outputs, std::vector<mg::DisplayConfigurationOutput> && config)
        : max_simultaneous_outputs{max_simultaneous_outputs},
        outputs{config}
    {
        if (max_simultaneous_outputs == max_simultaneous_outputs_all)
            max_simultaneous_outputs = outputs.size();
    }

    MockDisplayConfiguration(std::vector<mg::DisplayConfigurationOutput> && config)
        : MockDisplayConfiguration(max_simultaneous_outputs_all, std::move(config))
    {
    }

    void for_each_card(std::function<void(mg::DisplayConfigurationCard const&)> f) const
    {
        f({mg::DisplayConfigurationCardId{1}, max_simultaneous_outputs});
    }

    void for_each_output(std::function<void(mg::DisplayConfigurationOutput const&)> f) const
    {
        for (auto const& output : outputs)
            f(output);
    }

    MOCK_METHOD6(configure_output, void(mg::DisplayConfigurationOutputId, bool,
                                        geom::Point, size_t, size_t, MirPowerMode));

    static const size_t max_simultaneous_outputs_all{std::numeric_limits<size_t>::max()};
private:
    size_t max_simultaneous_outputs;
    std::vector<mg::DisplayConfigurationOutput> outputs;
};

}

mg::DisplayConfigurationOutput default_output(mg::DisplayConfigurationOutputId id)
{
    return { id, mg::DisplayConfigurationCardId{1},
        mg::DisplayConfigurationOutputType::vga,
        {mir_pixel_format_abgr_8888},
        { {geom::Size{523, 555}, 60.0} },
        0,
        geom::Size{324, 642},
        true,
        false,
        geom::Point{geom::X{123}, geom::Y{343}},
        0,
        0,
        mir_power_mode_on
    };
}

mg::DisplayConfigurationOutput connected_with_modes()
{
    mg::DisplayConfigurationOutput output = default_output(mg::DisplayConfigurationOutputId{10}) ;
    output.modes = 
    {
        {geom::Size{123, 111}, 59.9},
        {geom::Size{123, 111}, 59.9},
        {geom::Size{123, 111}, 59.9}
    };
    output.preferred_mode_index = 2;
    output.current_mode_index = 1;
    return output;
}
mg::DisplayConfigurationOutput connected_without_modes()
{
    mg::DisplayConfigurationOutput output = default_output(mg::DisplayConfigurationOutputId{11});
    output.pixel_formats = {};
    output.modes = {};
    output.current_format_index = std::numeric_limits<size_t>::max();
    output.current_mode_index = std::numeric_limits<size_t>::max();
    return output;
}

mg::DisplayConfigurationOutput connected_with_single_mode()
{
    return default_output(mg::DisplayConfigurationOutputId{12});
}

mg::DisplayConfigurationOutput not_connected()
{
    mg::DisplayConfigurationOutput output = default_output(mg::DisplayConfigurationOutputId{13});
    output.connected = false;
    output.current_mode_index = 1;
    return output;
}

mg::DisplayConfigurationOutput connected_with_rgba_and_xrgb()
{
    mg::DisplayConfigurationOutput output = default_output(mg::DisplayConfigurationOutputId{14});
    output.pixel_formats = {mir_pixel_format_argb_8888, mir_pixel_format_xrgb_8888};
    return output;
}

mg::DisplayConfigurationOutput connected_with_xrgb_bgr()
{
    mg::DisplayConfigurationOutput output = default_output(mg::DisplayConfigurationOutputId{15});
    output.pixel_formats = {mir_pixel_format_xrgb_8888, mir_pixel_format_bgr_888};
    output.current_format_index = 1;
    return output;
}

MockDisplayConfiguration create_default_configuration(size_t max_outputs = MockDisplayConfiguration::max_simultaneous_outputs_all)
{
    return MockDisplayConfiguration
    {
        max_outputs,
        {
            connected_with_modes(),
            connected_without_modes(),
            connected_with_single_mode(),
            not_connected(),
        }
    };
}

TEST(DefaultDisplayConfigurationPolicyTest, uses_all_connected_valid_outputs)
{
    using namespace ::testing;

    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration conf{create_default_configuration()};

    conf.for_each_output([&conf](mg::DisplayConfigurationOutput const& output)
    {
        if (output.connected && output.modes.size() > 0)
        {
            EXPECT_CALL(conf, configure_output(output.id, true, geom::Point(),
                                               output.preferred_mode_index, 
                                               _, _));
        }
        else
        {
            EXPECT_CALL(conf, configure_output(output.id, false, output.top_left,
                                               output.current_mode_index,
                                               _, _));
        }
    });

    policy.apply_to(conf);
}

TEST(DefaultDisplayConfigurationPolicyTest, default_policy_is_power_mode_on)
{
    using namespace ::testing;

    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration conf{create_default_configuration()};

    conf.for_each_output([&conf](mg::DisplayConfigurationOutput const& output)
    {
        EXPECT_CALL(conf, configure_output(output.id, _, _, _, _, mir_power_mode_on));
    });

    policy.apply_to(conf);
}

TEST(DefaultDisplayConfigurationPolicyTest, does_not_enable_more_outputs_than_supported)
{
    using namespace ::testing;

    size_t const max_simultaneous_outputs{1};
    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration conf{create_default_configuration(max_simultaneous_outputs)};

    size_t output_count{0};
    conf.for_each_output([&output_count](mg::DisplayConfigurationOutput const&)
    {
        ++output_count;
    });

    EXPECT_CALL(conf, configure_output(_, true, _, _, _, _))
        .Times(AtMost(max_simultaneous_outputs));

    EXPECT_CALL(conf, configure_output(_, false, _, _, _, _))
        .Times(AtLeast(output_count - max_simultaneous_outputs));

    policy.apply_to(conf);
}

TEST(DefaultDisplayConfigurationPolicyTest, prefer_opaque_over_alpha)
{
    using namespace ::testing;

    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration pick_xrgb{ { connected_with_rgba_and_xrgb() } };

    EXPECT_CALL(pick_xrgb, configure_output(_, true, _, _, 1, _));
    policy.apply_to(pick_xrgb);
}

TEST(DefaultDisplayConfigurationPolicyTest, preserve_opaque_selection)
{
    using namespace ::testing;

    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration keep_bgr{ { connected_with_xrgb_bgr() } };

    EXPECT_CALL(keep_bgr, configure_output(_, true, _, _, 1, _));
    policy.apply_to(keep_bgr);
}

TEST(DefaultDisplayConfigurationPolicyTest, accept_transparency_when_only_option)
{
    using namespace ::testing;

    mg::DefaultDisplayConfigurationPolicy policy;
    MockDisplayConfiguration pick_rgba{ { default_output(mg::DisplayConfigurationOutputId{15}) } };

    EXPECT_CALL(pick_rgba, configure_output(_, true, _, _, 0, _));
    policy.apply_to(pick_rgba);
}

