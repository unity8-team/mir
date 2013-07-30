/*
 * Copyright © 2012 Canonical Ltd.
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

#include "mir_toolkit/mir_client_library.h"
#include "src/client/android/android_client_buffer_factory.h"
#include "src/client/android/android_client_buffer.h"
#include "mir_test_doubles/mock_android_registrar.h"

#include <gtest/gtest.h>

namespace geom = mir::geometry;
namespace mcla = mir::client::android;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;


struct MirBufferFactoryTest : public testing::Test
{
    void SetUp()
    {
        width = geom::Width(12);
        height =geom::Height(14);
        size = geom::Size{width, height};
        pf = geom::PixelFormat::abgr_8888;

        mock_registrar = std::make_shared<mtd::MockAndroidRegistrar>();
        package1 = std::make_shared<MirBufferPackage>();
        package2 = std::make_shared<MirBufferPackage>();

    }
    geom::Width width;
    geom::Height height;
    geom::PixelFormat pf;
    geom::Size size;

    std::shared_ptr<mtd::MockAndroidRegistrar> mock_registrar;

    std::shared_ptr<MirBufferPackage> package1;
    std::shared_ptr<MirBufferPackage> package2;

};

TEST_F(MirBufferFactoryTest, factory_sets_width_and_height)
{
    using namespace testing;

    mcla::AndroidClientBufferFactory buffer_factory(mock_registrar);

    EXPECT_CALL(*mock_registrar, register_buffer(_))
        .Times(1);
    EXPECT_CALL(*mock_registrar, unregister_buffer(_))
        .Times(1);

    auto buffer = buffer_factory.create_buffer(package1, size, pf);

    EXPECT_EQ(buffer->size().height, height);
    EXPECT_EQ(buffer->size().width, width);
    EXPECT_EQ(buffer->pixel_format(), pf);
}
