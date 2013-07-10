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

#include <boost/throw_exception.hpp>
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/display.h"
#include "src/server/graphics/gbm/gbm_platform.h"
#include "src/server/graphics/gbm/kms_display_configuration.h"

#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir/graphics/null_display_report.h"
#include "mir_test_doubles/null_virtual_terminal.h"

#include "mir_test_framework/udev_environment.h"

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;
namespace mtf = mir::mir_test_framework;

namespace
{

mg::DisplayConfigurationMode conf_mode_from_drm_mode(drmModeModeInfo const& mode)
{
    geom::Size const size{geom::Width{mode.hdisplay}, geom::Height{mode.vdisplay}};
    double vrefresh_hz{0.0};

    /* Calculate vertical refresh rate from DRM mode information */
    if (mode.htotal != 0.0 && mode.vtotal != 0.0)
    {
        vrefresh_hz = mode.clock * 1000.0 / (mode.htotal * mode.vtotal);
        vrefresh_hz = round(vrefresh_hz * 10.0) / 10.0;
    }

    return mg::DisplayConfigurationMode{size, vrefresh_hz};
}

class GBMDisplayConfigurationTest : public ::testing::Test
{
public:
    GBMDisplayConfigurationTest()
    {
        using namespace testing;

        /* Needed for display start-up */
        ON_CALL(mock_egl, eglChooseConfig(_,_,_,1,_))
        .WillByDefault(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                             SetArgPointee<4>(1),
                             Return(EGL_TRUE)));

        const char* egl_exts = "EGL_KHR_image EGL_KHR_image_base EGL_MESA_drm_image";
        const char* gl_exts = "GL_OES_texture_npot GL_OES_EGL_image";

        ON_CALL(mock_egl, eglQueryString(_,EGL_EXTENSIONS))
        .WillByDefault(Return(egl_exts));
        ON_CALL(mock_gl, glGetString(GL_EXTENSIONS))
        .WillByDefault(Return(reinterpret_cast<const GLubyte*>(gl_exts)));

        setup_sample_modes();

        fake_devices.add_standard_drm_devices();
    }

    std::shared_ptr<mgg::GBMPlatform> create_platform()
    {
        return std::make_shared<mgg::GBMPlatform>(
            std::make_shared<mg::NullDisplayReport>(),
            std::make_shared<mtd::NullVirtualTerminal>());
    }

    void setup_sample_modes()
    {
        /* Add DRM modes */
        modes0.push_back(mtd::FakeDRMResources::create_mode(1920, 1080, 138500, 2080, 1111));
        modes0.push_back(mtd::FakeDRMResources::create_mode(1920, 1080, 148500, 2200, 1125));
        modes0.push_back(mtd::FakeDRMResources::create_mode(1680, 1050, 119000, 1840, 1080));
        modes0.push_back(mtd::FakeDRMResources::create_mode(832, 624, 57284, 1152, 667));

        /* Add the DisplayConfiguration modes corresponding to the DRM modes */
        for (auto const& mode : modes0)
            conf_modes0.push_back(conf_mode_from_drm_mode(mode));
    }

    ::testing::NiceMock<mtd::MockEGL> mock_egl;
    ::testing::NiceMock<mtd::MockGL> mock_gl;
    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;

    std::vector<drmModeModeInfo> modes0;
    std::vector<mg::DisplayConfigurationMode> conf_modes0;
    std::vector<drmModeModeInfo> modes_empty;

    mtf::UdevEnvironment fake_devices;
};

}

TEST_F(GBMDisplayConfigurationTest, configuration_is_read_correctly)
{
    using namespace ::testing;

    /* Set up DRM resources */
    uint32_t const invalid_id{0};
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const encoder1_id{21};
    uint32_t const connector0_id{30};
    uint32_t const connector1_id{31};
    uint32_t const connector2_id{32};
    geom::Size const connector0_physical_size_mm{geom::Width{480}, geom::Height{270}};
    geom::Size const connector1_physical_size_mm{};
    geom::Size const connector2_physical_size_mm{};
    std::vector<uint32_t> possible_encoder_ids_empty;
    uint32_t const possible_crtcs_mask_empty{0};

    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);

    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    resources.add_encoder(encoder1_id, invalid_id, possible_crtcs_mask_empty);

    resources.add_connector(connector0_id, DRM_MODE_CONNECTED, encoder0_id,
                            modes0, possible_encoder_ids_empty,
                            connector0_physical_size_mm);
    resources.add_connector(connector1_id, DRM_MODE_DISCONNECTED, invalid_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector1_physical_size_mm);
    resources.add_connector(connector2_id, DRM_MODE_DISCONNECTED, encoder1_id,
                            modes_empty, possible_encoder_ids_empty,
                            connector2_physical_size_mm);

    resources.prepare();

    /* Expected results */
    std::vector<mg::DisplayConfigurationOutput> const expected_outputs =
    {
        {
            mg::DisplayConfigurationOutputId{connector0_id},
            mg::DisplayConfigurationCardId{0},
            conf_modes0,
            connector0_physical_size_mm,
            true,
            1
        },
        {
            mg::DisplayConfigurationOutputId{connector1_id},
            mg::DisplayConfigurationCardId{0},
            std::vector<mg::DisplayConfigurationMode>(),
            connector1_physical_size_mm,
            false,
            std::numeric_limits<size_t>::max()
        },
        {
            mg::DisplayConfigurationOutputId{connector2_id},
            mg::DisplayConfigurationCardId{0},
            std::vector<mg::DisplayConfigurationMode>(),
            connector2_physical_size_mm,
            false,
            std::numeric_limits<size_t>::max()
        }
    };

    /* Test body */
    auto platform = create_platform();
    auto display = platform->create_display();

    auto conf = display->configuration();

    size_t output_count{0};

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, expected_outputs.size());
        EXPECT_EQ(expected_outputs[output_count], output) << "output_count: " << output_count;
        ++output_count;
    });

    EXPECT_EQ(expected_outputs.size(), output_count);
}

TEST_F(GBMDisplayConfigurationTest, get_kms_connector_id_returns_correct_id)
{
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const possible_crtcs_mask_empty{0};
    std::vector<uint32_t> const connector_ids{30, 31};
    std::vector<uint32_t> encoder_ids{20};

    /* Set up DRM resources */
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);
    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    for (auto id : connector_ids)
    {
        resources.add_connector(id, DRM_MODE_CONNECTED, encoder0_id,
                                modes0, encoder_ids,
                                geom::Size());
    }

    resources.prepare();

    /* Test body */
    auto platform = create_platform();
    auto display = platform->create_display();

    auto conf = display->configuration();
    auto const& kms_conf = std::static_pointer_cast<mgg::KMSDisplayConfiguration>(conf);

    size_t output_count{0};

    conf->for_each_output([&](mg::DisplayConfigurationOutput const& output)
    {
        ASSERT_LT(output_count, connector_ids.size());

        EXPECT_EQ(connector_ids[output_count],
                  kms_conf->get_kms_connector_id(output.id));
        ++output_count;
    });
}

TEST_F(GBMDisplayConfigurationTest, get_kms_connector_id_throws_on_invalid_id)
{
    uint32_t const crtc0_id{10};
    uint32_t const encoder0_id{20};
    uint32_t const possible_crtcs_mask_empty{0};
    std::vector<uint32_t> const connector_ids{30, 31};
    std::vector<uint32_t> encoder_ids{20};

    /* Set up DRM resources */
    mtd::FakeDRMResources& resources(mock_drm.fake_drm);

    resources.reset();

    resources.add_crtc(crtc0_id, modes0[1]);
    resources.add_encoder(encoder0_id, crtc0_id, possible_crtcs_mask_empty);
    for (auto id : connector_ids)
    {
        resources.add_connector(id, DRM_MODE_CONNECTED, encoder0_id,
                                modes0, encoder_ids,
                                geom::Size());
    }

    resources.prepare();

    /* Test body */
    auto platform = create_platform();
    auto display = platform->create_display();

    auto conf = display->configuration();
    auto const& kms_conf = std::static_pointer_cast<mgg::KMSDisplayConfiguration>(conf);

    EXPECT_THROW({
        kms_conf->get_kms_connector_id(mg::DisplayConfigurationOutputId{29});
    }, std::runtime_error);
    EXPECT_THROW({
        kms_conf->get_kms_connector_id(mg::DisplayConfigurationOutputId{32});
    }, std::runtime_error);
}
