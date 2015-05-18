/*
 * Copyright © 2015 Canonical Ltd.
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

#include "src/platforms/android/server/configurable_display_buffer.h"
#include "src/platforms/android/server/display_group.h"
#include "mir_test_doubles/mock_display_device.h"
#include "mir_test_doubles/stub_renderable_list_compositor.h"
#include "mir_test_doubles/stub_swapping_gl_context.h"
#include "mir_test/fake_shared.h"
#include <memory>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;
namespace mt=mir::test;

namespace
{
struct StubConfigurableDB : mga::ConfigurableDisplayBuffer
{
    mir::geometry::Rectangle view_area() const override { return {}; }
    void make_current() override {}
    void release_current() override {}
    void gl_swap_buffers() override {}
    bool post_renderables_if_optimizable(mg::RenderableList const&) override { return false; }
    MirOrientation orientation() const override { return mir_orientation_normal; }
    void configure(MirPowerMode, MirOrientation) override {}
    mga::DisplayContents contents() override
    {
        return mga::DisplayContents{mga::DisplayName::primary, list, context, compositor};
    }
    MirPowerMode power_mode() const override { return mir_power_mode_on; }
    mga::LayerList mutable list{std::make_shared<mga::IntegerSourceCrop>(), {}};
    mtd::StubRenderableListCompositor mutable compositor;
    mtd::StubSwappingGLContext mutable context;
};
}

TEST(DisplayGroup, db_additions_and_removals)
{
    using namespace testing;
    NiceMock<mtd::MockDisplayDevice> mock_device;
    InSequence seq;
    EXPECT_CALL(mock_device, commit(SizeIs(1)));
    EXPECT_CALL(mock_device, commit(SizeIs(2)));
    EXPECT_CALL(mock_device, commit(SizeIs(2)));
    EXPECT_CALL(mock_device, commit(SizeIs(1)));

    mga::DisplayGroup group(mt::fake_shared(mock_device), std::unique_ptr<StubConfigurableDB>{new StubConfigurableDB});
    group.post();
 
    //hwc api does not allow removing primary
    EXPECT_THROW({
        group.remove(mga::DisplayName::primary);
    }, std::logic_error);

    EXPECT_FALSE(group.display_present(mga::DisplayName::external));
    group.add(mga::DisplayName::external, std::unique_ptr<StubConfigurableDB>(new StubConfigurableDB));
    EXPECT_TRUE(group.display_present(mga::DisplayName::external));
    group.post();

    //can replace primary or external
    group.add(mga::DisplayName::primary, std::unique_ptr<StubConfigurableDB>(new StubConfigurableDB));
    group.add(mga::DisplayName::external, std::unique_ptr<StubConfigurableDB>(new StubConfigurableDB));
    group.post();
    group.remove(mga::DisplayName::external);
    group.remove(mga::DisplayName::external);
    group.post();
}
