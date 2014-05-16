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

#include "src/platform/graphics/android/hwc_layerlist.h"
#include "mir_test_doubles/mock_buffer.h"
#include "hwc_struct_helpers.h"
#include "mir_test_doubles/mock_android_native_buffer.h"
#include "mir_test_doubles/mock_renderable.h"
#include "mir_test/fake_shared.h"
#include <gtest/gtest.h>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mt=mir::test;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

class HWCLayersTest : public ::testing::Test
{
public:
    virtual void SetUp()
    {
        using namespace testing;

        native_handle_1 = std::make_shared<mtd::StubAndroidNativeBuffer>(buffer_size);
        ON_CALL(mock_buffer, size())
            .WillByDefault(Return(buffer_size));
        ON_CALL(mock_buffer, native_buffer_handle())
            .WillByDefault(Return(native_handle_1));

        list = std::shared_ptr<hwc_display_contents_1_t>(
            static_cast<hwc_display_contents_1_t*>(
                ::operator new(sizeof(hwc_display_contents_1_t) + sizeof(hwc_layer_1_t))));
        list_index = 0;
        hwc_layer = &list->hwLayers[list_index];
        type = mga::LayerType::gl_rendered;

        reset_expected_layer();
    }

    void reset_expected_layer()
    {
        memset(&expected_layer, 0, sizeof(expected_layer));
        expected_layer.compositionType = HWC_FRAMEBUFFER;
        expected_layer.hints = 0;
        expected_layer.flags = 0;
        expected_layer.handle = nullptr;
        expected_layer.transform = 0;
        expected_layer.blending = HWC_BLENDING_NONE;
        expected_layer.sourceCrop = region;
        expected_layer.displayFrame = {
            screen_position.top_left.x.as_int(),
            screen_position.top_left.y.as_int(),
            screen_position.bottom_right().x.as_int(),
            screen_position.bottom_right().y.as_int()};
        expected_layer.visibleRegionScreen = {1, &region};
        expected_layer.acquireFenceFd = -1;
        expected_layer.releaseFenceFd = -1;
    }

    mga::LayerType type;
    geom::Size buffer_size{333, 444};
    geom::Rectangle screen_position{{9,8},buffer_size};
    bool alpha_enabled{false};
    std::shared_ptr<mtd::StubAndroidNativeBuffer> native_handle_1;
    testing::NiceMock<mtd::MockBuffer> mock_buffer;

    std::shared_ptr<hwc_display_contents_1_t> list;
    hwc_layer_1_t* hwc_layer;
    size_t list_index;
    hwc_layer_1 expected_layer;
    hwc_rect_t region{0,0,0,0};
};

TEST_F(HWCLayersTest, check_if_layer_needs_gl_render)
{
    mga::HWCLayer layer(list, list_index);

    hwc_layer->compositionType = HWC_OVERLAY;
    hwc_layer->flags = 0;
    EXPECT_FALSE(layer.needs_gl_render());

    hwc_layer->compositionType = HWC_FRAMEBUFFER;
    hwc_layer->flags = HWC_SKIP_LAYER;
    EXPECT_TRUE(layer.needs_gl_render());

    hwc_layer->compositionType = HWC_FRAMEBUFFER;
    hwc_layer->flags = 0;
    EXPECT_TRUE(layer.needs_gl_render());
}

TEST_F(HWCLayersTest, move_layer_positions)
{
    mga::HWCLayer layer(type, screen_position, false, list, list_index);
    mga::HWCLayer second_layer(std::move(layer));

    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));
}

TEST_F(HWCLayersTest, change_layer_types)
{
    expected_layer.compositionType = HWC_FRAMEBUFFER_TARGET;

    type = mga::LayerType::framebuffer_target;
    mga::HWCLayer layer(type, screen_position, alpha_enabled, list, list_index);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    EXPECT_THROW({
        layer.set_layer_type(mga::LayerType::overlay);
    }, std::logic_error);

    expected_layer.compositionType = HWC_FRAMEBUFFER;
    layer.set_layer_type(mga::LayerType::gl_rendered);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    expected_layer.compositionType = HWC_FRAMEBUFFER;
    expected_layer.flags = HWC_SKIP_LAYER;
    layer.set_layer_type(mga::LayerType::skip);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    expected_layer.compositionType = HWC_FRAMEBUFFER;
    expected_layer.flags = 0;
    layer.set_layer_type(mga::LayerType::gl_rendered);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));
}

TEST_F(HWCLayersTest, apply_buffer_updates_to_framebuffer_layer)
{
    EXPECT_CALL(*native_handle_1, copy_fence())
        .Times(0);

    hwc_rect_t region = {0,0,buffer_size.width.as_int(), buffer_size.height.as_int()};
    expected_layer.handle = native_handle_1->handle();
    expected_layer.visibleRegionScreen = {1, &region};
    expected_layer.sourceCrop = region;
    expected_layer.acquireFenceFd = -1;
    expected_layer.releaseFenceFd = -1;

    mga::HWCLayer layer(type, screen_position, alpha_enabled, list, list_index);
    layer.set_buffer(mock_buffer);

    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));
}

TEST_F(HWCLayersTest, apply_buffer_updates_to_overlay_layers)
{
    int fake_fence = 552;
    hwc_rect_t region = {0,0,buffer_size.width.as_int(), buffer_size.height.as_int()};
    expected_layer.compositionType = HWC_OVERLAY;
    expected_layer.handle = native_handle_1->handle();
    expected_layer.visibleRegionScreen = {1, &region};
    expected_layer.sourceCrop = region;
    expected_layer.acquireFenceFd = -1;
    expected_layer.releaseFenceFd = -1;

    mga::HWCLayer layer(type, screen_position, alpha_enabled, list, list_index);
    hwc_layer->compositionType = HWC_OVERLAY;

    layer.set_buffer(mock_buffer);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    //prepare_for_draw should set the fence
    //mir must reset releaseFenceFd to -1
    hwc_layer->releaseFenceFd = fake_fence;
    EXPECT_CALL(*native_handle_1, copy_fence())
        .Times(1)
        .WillOnce(testing::Return(fake_fence));
    expected_layer.acquireFenceFd = fake_fence;
    layer.prepare_for_draw();
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    //multiple sequential updates to the same layer must not set the acquireFenceFds on the calls
    //after the first.
    hwc_layer->acquireFenceFd = -1;
    expected_layer.acquireFenceFd = -1;
    layer.set_buffer(mock_buffer);
    layer.prepare_for_draw();
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));
}

TEST_F(HWCLayersTest, apply_buffer_updates_to_fbtarget)
{
    int fake_fence = 552;
    hwc_rect_t region = {0,0,buffer_size.width.as_int(), buffer_size.height.as_int()};
    expected_layer.compositionType = HWC_FRAMEBUFFER_TARGET;
    expected_layer.handle = native_handle_1->handle();
    expected_layer.visibleRegionScreen = {1, &region};
    expected_layer.sourceCrop = region;
    expected_layer.acquireFenceFd = -1;
    expected_layer.releaseFenceFd = -1;

    mga::HWCLayer layer(
        mga::LayerType::framebuffer_target, screen_position, alpha_enabled, list, list_index);

    layer.set_buffer(mock_buffer);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    //mir must reset releaseFenceFd to -1 if hwc has changed it
    hwc_layer->releaseFenceFd = fake_fence;
    EXPECT_CALL(*native_handle_1, copy_fence())
        .Times(1)
        .WillOnce(testing::Return(fake_fence));
    expected_layer.acquireFenceFd = fake_fence;
    layer.prepare_for_draw();
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    //hwc will set this to -1 to acknowledge that its adopted this layer's fence.
    //multiple sequential updates to the same layer must not set the acquireFenceFds on the calls
    //after the first.
    hwc_layer->acquireFenceFd = -1;
    expected_layer.acquireFenceFd = -1;
    layer.set_buffer(mock_buffer);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    //TODO: we have to know if the fb target is needed or not. if it is not, we should not copy the fd. 
}

TEST_F(HWCLayersTest, buffer_fence_updates)
{
    int fake_fence = 552;
    EXPECT_CALL(*native_handle_1, update_fence(fake_fence))
        .Times(1);
    type = mga::LayerType::framebuffer_target;
    mga::HWCLayer layer(type, screen_position, alpha_enabled, list, list_index);

    layer.set_buffer(mock_buffer);
    hwc_layer->releaseFenceFd = fake_fence;
    layer.update_fence_and_release_buffer();
}

TEST_F(HWCLayersTest, check_layer_defaults_and_alpha)
{
    using namespace testing;

    hwc_rect_t crop
    {
        0,0,
        buffer_size.width.as_int(),
        buffer_size.height.as_int()
    };

    hwc_rect_t screen_pos
    {
        screen_position.top_left.x.as_int(),
        screen_position.top_left.y.as_int(),
        screen_position.bottom_right().x.as_int(),
        screen_position.bottom_right().y.as_int()
    };

    hwc_region_t visible_region {1, &screen_pos};
    hwc_layer_1 expected_layer;

    expected_layer.compositionType = HWC_FRAMEBUFFER;
    expected_layer.hints = 0;
    expected_layer.flags = 0;
    expected_layer.handle = native_handle_1->handle();
    expected_layer.transform = 0;
    expected_layer.blending = HWC_BLENDING_COVERAGE;
    expected_layer.sourceCrop = crop;
    expected_layer.displayFrame = screen_pos;
    expected_layer.visibleRegionScreen = visible_region;
    expected_layer.acquireFenceFd = -1;
    expected_layer.releaseFenceFd = -1;

    mga::HWCLayer layer(list, list_index);
    layer.set_render_parameters(screen_position, true);
    layer.set_buffer(mock_buffer);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));

    expected_layer.blending = HWC_BLENDING_NONE;
    layer.set_render_parameters(screen_position, false);
    EXPECT_THAT(*hwc_layer, MatchesLayer(expected_layer));
}
