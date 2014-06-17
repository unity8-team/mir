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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/platform/graphics/android/hwc_fb_device.h"
#include "mir_test_doubles/mock_display_device.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_android_native_buffer.h"
#include "mir_test_doubles/mock_hwc_vsync_coordinator.h"
#include "mir_test_doubles/mock_framebuffer_bundle.h"
#include "mir_test_doubles/mock_fb_hal_device.h"
#include "mir_test_doubles/stub_renderable.h"
#include "mir_test_doubles/stub_swapping_gl_context.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_hwc_device_wrapper.h"
#include "mir_test_doubles/stub_renderable_list_compositor.h"
#include "mir_test_doubles/mock_renderable_list_compositor.h"
#include "src/platform/graphics/android/hwc_fallback_gl_renderer.h"
#include "hwc_struct_helpers.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

class HwcFbDevice : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        using namespace testing;
        int width = 88;
        int height = 4;
        test_size = geom::Size{width, height};
        int fbnum = 558;
        mock_hwc_device = std::make_shared<testing::NiceMock<mtd::MockHWCComposerDevice1>>();
        mock_fb_device = std::make_shared<mtd::MockFBHalDevice>(
            width, height, HAL_PIXEL_FORMAT_RGBA_8888, fbnum);
        mock_vsync = std::make_shared<testing::NiceMock<mtd::MockVsyncCoordinator>>();
        mock_buffer = std::make_shared<NiceMock<mtd::MockBuffer>>();
        mock_hwc_device_wrapper = std::make_shared<testing::NiceMock<mtd::MockHWCDeviceWrapper>>();

        hwc_rect_t empty_region = {0,0,0,0};
        skip_layer.compositionType = HWC_FRAMEBUFFER;
        skip_layer.hints = 0;
        skip_layer.flags = HWC_SKIP_LAYER;
        skip_layer.handle = 0;
        skip_layer.transform = 0;
        skip_layer.blending = HWC_BLENDING_NONE;
        skip_layer.sourceCrop = empty_region;
        skip_layer.displayFrame = empty_region;
        skip_layer.visibleRegionScreen = {1, &empty_region};
        skip_layer.acquireFenceFd = -1;
        skip_layer.releaseFenceFd = -1;
    }

    int fake_dpy = 0;
    int fake_sur = 0;
    EGLDisplay dpy{&fake_dpy};
    EGLSurface sur{&fake_sur};

    testing::NiceMock<mtd::MockEGL> mock_egl;

    geom::Size test_size;
    std::shared_ptr<mtd::MockHWCComposerDevice1> mock_hwc_device;
    std::shared_ptr<mtd::MockFBHalDevice> mock_fb_device;
    std::shared_ptr<mtd::MockVsyncCoordinator> mock_vsync;
    std::shared_ptr<mtd::MockBuffer> mock_buffer;
    std::shared_ptr<mtd::MockHWCDeviceWrapper> mock_hwc_device_wrapper;
    mtd::StubSwappingGLContext stub_context;
    hwc_layer_1_t skip_layer;
};

TEST_F(HwcFbDevice, hwc10_render_gl_only)
{
    using namespace testing;
    std::list<hwc_layer_1_t*> expected_list{&skip_layer};

    Sequence seq;
    EXPECT_CALL(*mock_hwc_device_wrapper, prepare(MatchesList(expected_list)))
        .InSequence(seq);
    EXPECT_CALL(mock_egl, eglGetCurrentDisplay())
        .InSequence(seq)
        .WillOnce(Return(dpy));
    EXPECT_CALL(mock_egl, eglGetCurrentSurface(EGL_DRAW))
        .InSequence(seq)
        .WillOnce(Return(sur));
    EXPECT_CALL(*mock_hwc_device_wrapper, set(MatchesListWithEglFields(expected_list, dpy, sur)))
        .InSequence(seq);

    mga::HwcFbDevice device(mock_hwc_device, mock_hwc_device_wrapper, mock_fb_device, mock_vsync);

    device.render_gl(stub_context);
}

TEST_F(HwcFbDevice, hwc10_prepare_with_renderables)
{
    using namespace testing;
    mtd::MockRenderableListCompositor mock_compositor;
    std::list<hwc_layer_1_t*> expected_list{&skip_layer};
    auto renderable1 = std::make_shared<mtd::StubRenderable>();
    auto renderable2 = std::make_shared<mtd::StubRenderable>();
    std::list<std::shared_ptr<mg::Renderable>> renderlist
    {
        renderable1,
        renderable2
    };

    testing::Sequence seq;
    EXPECT_CALL(*mock_hwc_device_wrapper, prepare(MatchesList(expected_list)))
        .InSequence(seq);
    EXPECT_CALL(mock_compositor, render(Ref(renderlist),_))
        .InSequence(seq)
        .WillOnce(Invoke([](mg::RenderableList const&, mga::SwappingGLContext const& cont)
        {
            cont.swap_buffers();
        }));
    EXPECT_CALL(mock_egl, eglGetCurrentDisplay())
        .InSequence(seq)
        .WillOnce(Return(dpy));
    EXPECT_CALL(mock_egl, eglGetCurrentSurface(EGL_DRAW))
        .InSequence(seq)
        .WillOnce(Return(sur));
    EXPECT_CALL(*mock_hwc_device_wrapper, set(MatchesListWithEglFields(expected_list, dpy, sur)))
        .InSequence(seq);

    mga::HwcFbDevice device(mock_hwc_device, mock_hwc_device_wrapper, mock_fb_device, mock_vsync);
    device.prepare_overlays(stub_context, renderlist, mock_compositor);
}

TEST_F(HwcFbDevice, hwc10_post)
{
    using namespace testing;
    auto native_buffer = std::make_shared<NiceMock<mtd::MockAndroidNativeBuffer>>();
    Sequence seq;
    EXPECT_CALL(*mock_buffer, native_buffer_handle())
        .InSequence(seq)
        .WillOnce(Return(native_buffer));
    EXPECT_CALL(*mock_fb_device, post_interface(mock_fb_device.get(), &native_buffer->native_handle))
        .InSequence(seq);
    EXPECT_CALL(*mock_vsync, wait_for_vsync())
        .InSequence(seq);
    mga::HwcFbDevice device(mock_hwc_device, mock_hwc_device_wrapper, mock_fb_device, mock_vsync);
    device.post(*mock_buffer);
}