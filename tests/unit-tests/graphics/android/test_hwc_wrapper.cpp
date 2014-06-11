/*
 * Copyright © 2014 Canonical Ltd.
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

#include "src/platform/graphics/android/real_hwc_wrapper.h"
#include "src/platform/graphics/android/hwc_logger.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;

namespace
{
struct MockHwcLogger : public mga::HwcLogger
{
    MOCK_CONST_METHOD1(log_list_submitted_to_prepare, void(hwc_display_contents_1_t const&));
    MOCK_CONST_METHOD1(log_prepare_done, void(hwc_display_contents_1_t const&));
    MOCK_CONST_METHOD1(log_set_list, void(hwc_display_contents_1_t const&));
    MOCK_CONST_METHOD1(log_overlay_optimization, void(mga::OverlayOptimization));
    MOCK_CONST_METHOD1(log_screen_blank, void(mga::HwcBlankCommand));
};
}

struct HwcWrapper : public ::testing::Test
{
    HwcWrapper()
     : mock_device(std::make_shared<testing::NiceMock<mtd::MockHWCComposerDevice1>>()),
       mock_logger(std::make_shared<testing::NiceMock<MockHwcLogger>>()),
       virtual_display{nullptr},
       external_display{nullptr},
       primary_display{nullptr}
    {
    }

    int display_saving_fn(
        struct hwc_composer_device_1*, size_t sz, hwc_display_contents_1_t** displays)
    {
        switch (sz)
        {
            case 3:
                virtual_display = displays[2];
            case 2:
                external_display = displays[1];
            case 1:
                primary_display = displays[0];
            default:
                break;
        }
        return 0;
    }

    hwc_display_contents_1_t list;
    std::shared_ptr<mtd::MockHWCComposerDevice1> const mock_device;
    std::shared_ptr<MockHwcLogger> const mock_logger;
    hwc_display_contents_1_t *virtual_display;
    hwc_display_contents_1_t *external_display;
    hwc_display_contents_1_t *primary_display;
};

TEST_F(HwcWrapper, submits_correct_prepare_parameters)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_logger, log_list_submitted_to_prepare(Ref(list)))
        .InSequence(seq);
    EXPECT_CALL(*mock_device, prepare_interface(mock_device.get(), 1, _))
        .InSequence(seq)
        .WillOnce(Invoke(this, &HwcWrapper::display_saving_fn));
    EXPECT_CALL(*mock_logger, log_prepare_done(Ref(list)))
        .InSequence(seq);

    mga::RealHwcWrapper wrapper(mock_device, mock_logger);
    wrapper.prepare(list);

    EXPECT_EQ(&list, primary_display);
    EXPECT_EQ(nullptr, virtual_display);
    EXPECT_EQ(nullptr, external_display);
}

TEST_F(HwcWrapper, throws_on_prepare_failure)
{
    using namespace testing;

    mga::RealHwcWrapper wrapper(mock_device, mock_logger);

    EXPECT_CALL(*mock_device, prepare_interface(mock_device.get(), _, _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        wrapper.prepare(list);
    }, std::runtime_error);
}

TEST_F(HwcWrapper, submits_correct_set_parameters)
{
    using namespace testing;
    Sequence seq;
    EXPECT_CALL(*mock_logger, log_set_list(Ref(list)))
        .InSequence(seq);
    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), 1, _))
        .InSequence(seq)
        .WillOnce(Invoke(this, &HwcWrapper::display_saving_fn));

    mga::RealHwcWrapper wrapper(mock_device, mock_logger);
    wrapper.set(list);

    EXPECT_EQ(&list, primary_display);
    EXPECT_EQ(nullptr, virtual_display);
    EXPECT_EQ(nullptr, external_display);
}

TEST_F(HwcWrapper, throws_on_set_failure)
{
    using namespace testing;

    mga::RealHwcWrapper wrapper(mock_device, mock_logger);

    EXPECT_CALL(*mock_device, set_interface(mock_device.get(), _, _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        wrapper.set(list);
    }, std::runtime_error);
}
