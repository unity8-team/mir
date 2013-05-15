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

#include "src/server/graphics/gbm/linux_virtual_terminal.h"
#include "mir/graphics/null_display_report.h"
#include "mir/main_loop.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_display_report.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>

#include <linux/vt.h>
#include <linux/kd.h>

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{

class MockVTFileOperations : public mgg::VTFileOperations
{
public:
    ~MockVTFileOperations() noexcept {}
    MOCK_METHOD2(open, int(char const*, int));
    MOCK_METHOD1(close, int(int));
    MOCK_METHOD3(ioctl, int(int, int, int));
    MOCK_METHOD3(ioctl, int(int, int, void*));
    MOCK_METHOD1(make_raw, int(int));
};

class MockMainLoop : public mir::MainLoop
{
public:
    ~MockMainLoop() noexcept {}

    void run() {}
    void stop() {}

    MOCK_METHOD2(register_signal_handler,
                 void(std::initializer_list<int>,
                      std::function<void(int)> const&));
};

ACTION_TEMPLATE(SetIoctlPointee,
                HAS_1_TEMPLATE_PARAMS(typename, T),
                AND_1_VALUE_PARAMS(param))
{
    *static_cast<T*>(arg2) = param;
}

MATCHER_P(ModeUsesSignal, sig, "")
{
    auto vtm = static_cast<vt_mode*>(arg);

    return vtm->mode == VT_PROCESS &&
           vtm->relsig == sig &&
           vtm->acqsig == sig;
}

}

class LinuxVirtualTerminalTest : public ::testing::Test
{
public:
    LinuxVirtualTerminalTest()
        : fake_vt_fd{5},
          fake_kd_mode{KD_TEXT},
          fake_vt_mode{VT_AUTO, 0, 0, 0, 0}
    {
    }

    void set_up_expectations_for_current_vt_search(int vt_num)
    {
        using namespace testing;

        int const tmp_fd1{3};
        int const tmp_fd2{4};

        vt_stat vts = vt_stat();
        vts.v_active = vt_num;

        EXPECT_CALL(mock_fops, open(StrEq("/dev/tty"), _))
            .WillOnce(Return(tmp_fd1));
        EXPECT_CALL(mock_fops, ioctl(tmp_fd1, VT_GETSTATE, An<void*>()))
            .WillOnce(Return(-1));
        EXPECT_CALL(mock_fops, close(tmp_fd1))
            .WillOnce(Return(0));

        EXPECT_CALL(mock_fops, open(StrEq("/dev/tty0"), _))
            .WillOnce(Return(tmp_fd2));
        EXPECT_CALL(mock_fops, ioctl(tmp_fd2, VT_GETSTATE, An<void*>()))
            .WillOnce(DoAll(SetIoctlPointee<vt_stat>(vts), Return(0)));
        EXPECT_CALL(mock_fops, close(tmp_fd2))
            .WillOnce(Return(0));
    }

    void set_up_expectations_for_vt_setup(int vt_num)
    {
        using namespace testing;

        std::stringstream ss;
        ss << "/dev/tty" << vt_num;

        EXPECT_CALL(mock_fops, open(StrEq(ss.str()), _))
            .WillOnce(Return(fake_vt_fd));

        EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, KDGETMODE, An<void*>()))
            .WillOnce(DoAll(SetIoctlPointee<int>(fake_kd_mode), Return(0)));
        EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_GETMODE, An<void*>()))
            .WillOnce(DoAll(SetIoctlPointee<vt_mode>(fake_vt_mode), Return(0)));
    }

    void set_up_expectations_for_switch_handler(int sig)
    {
        using namespace testing;

        EXPECT_CALL(mock_main_loop, register_signal_handler(ElementsAre(sig), _))
            .WillOnce(SaveArg<1>(&sig_handler));
        EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_SETMODE,
                                     MatcherCast<void*>(ModeUsesSignal(sig))));
    }

    void set_up_expectations_for_vt_teardown()
    {
        using namespace testing;

        EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, KDSETMODE, fake_kd_mode))
            .WillOnce(Return(0));
        EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_SETMODE,An<void*>()))
            .WillOnce(Return(0));
        EXPECT_CALL(mock_fops, close(fake_vt_fd))
            .WillOnce(Return(0));
    }

    int const fake_vt_fd;
    int const fake_kd_mode;
    vt_mode fake_vt_mode;
    std::function<void(int)> sig_handler;
    MockVTFileOperations mock_fops;
    MockMainLoop mock_main_loop;
};


TEST_F(LinuxVirtualTerminalTest, sets_up_current_vt)
{
    using namespace testing;

    int const vt_num{7};

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);
    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    mgg::LinuxVirtualTerminal vt{fops, null_report};
}

TEST_F(LinuxVirtualTerminalTest, failure_to_find_current_vt_throws)
{
    using namespace testing;

    int const tmp_fd1{3};

    InSequence s;

    EXPECT_CALL(mock_fops, open(StrEq("/dev/tty"), _))
        .WillOnce(Return(tmp_fd1));
    EXPECT_CALL(mock_fops, ioctl(tmp_fd1, VT_GETSTATE, An<void*>()))
        .WillOnce(Return(-1));
    EXPECT_CALL(mock_fops, close(tmp_fd1))
        .WillOnce(Return(0));

    EXPECT_CALL(mock_fops, open(StrEq("/dev/tty0"), _))
        .WillOnce(Return(-1))
        .WillOnce(Return(-1));

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    EXPECT_THROW({
        mgg::LinuxVirtualTerminal vt(fops, null_report);
    }, std::runtime_error);
}

TEST_F(LinuxVirtualTerminalTest, sets_graphics_mode)
{
    using namespace testing;

    int const vt_num{7};

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);

    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, KDSETMODE, KD_GRAPHICS))
        .WillOnce(Return(0));

    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    mgg::LinuxVirtualTerminal vt(fops, null_report);
    vt.set_graphics_mode();
}

TEST_F(LinuxVirtualTerminalTest, failure_to_set_graphics_mode_throws)
{
    using namespace testing;

    int const vt_num{7};

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);

    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, KDSETMODE, KD_GRAPHICS))
        .WillOnce(Return(-1));

    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    mgg::LinuxVirtualTerminal vt(fops, null_report);
    EXPECT_THROW({
        vt.set_graphics_mode();
    }, std::runtime_error);
}


TEST_F(LinuxVirtualTerminalTest, uses_sigusr1_for_switch_handling)
{
    using namespace testing;

    int const vt_num{7};

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);
    set_up_expectations_for_switch_handler(SIGUSR1);
    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    mgg::LinuxVirtualTerminal vt(fops, null_report);

    auto null_handler = [] { return true; };
    vt.register_switch_handlers(mock_main_loop, null_handler, null_handler);
}

TEST_F(LinuxVirtualTerminalTest, allows_vt_switch_on_switch_away_handler_success)
{
    using namespace testing;

    int const vt_num{7};
    int const allow_switch{1};

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);
    set_up_expectations_for_switch_handler(SIGUSR1);

    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_RELDISP, allow_switch));

    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);
    auto null_report = std::make_shared<mg::NullDisplayReport>();

    mgg::LinuxVirtualTerminal vt(fops, null_report);

    auto succeeding_handler = [] { return true; };
    vt.register_switch_handlers(mock_main_loop, succeeding_handler, succeeding_handler);

    /* Fake a VT switch away request */
    sig_handler(SIGUSR1);
}

TEST_F(LinuxVirtualTerminalTest, disallows_vt_switch_on_switch_away_handler_failure)
{
    using namespace testing;

    int const vt_num{7};
    int const disallow_switch{0};
    mtd::MockDisplayReport mock_report;

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);
    set_up_expectations_for_switch_handler(SIGUSR1);

    /* First switch away attempt */
    EXPECT_CALL(mock_report, report_vt_switch_away_failure());
    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_RELDISP,
                                 MatcherCast<int>(disallow_switch)));

    /* Second switch away attempt */
    EXPECT_CALL(mock_report, report_vt_switch_away_failure());
    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_RELDISP,
                                 MatcherCast<int>(disallow_switch)));

    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);

    mgg::LinuxVirtualTerminal vt(fops, mt::fake_shared(mock_report));

    auto failing_handler = [] { return false; };
    vt.register_switch_handlers(mock_main_loop, failing_handler, failing_handler);

    /* Fake a VT switch away request */
    sig_handler(SIGUSR1);
    /* Fake another VT switch away request */
    sig_handler(SIGUSR1);
}

TEST_F(LinuxVirtualTerminalTest, reports_failed_vt_switch_back_attempt)
{
    using namespace testing;

    int const vt_num{7};
    int const allow_switch{1};
    mtd::MockDisplayReport mock_report;

    InSequence s;

    set_up_expectations_for_current_vt_search(vt_num);
    set_up_expectations_for_vt_setup(vt_num);
    set_up_expectations_for_switch_handler(SIGUSR1);

    /* Switch away */
    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_RELDISP, allow_switch));

    /* Switch back */
    EXPECT_CALL(mock_report, report_vt_switch_back_failure());
    EXPECT_CALL(mock_fops, ioctl(fake_vt_fd, VT_RELDISP, VT_ACKACQ));

    set_up_expectations_for_vt_teardown();

    auto fops = mt::fake_shared<mgg::VTFileOperations>(mock_fops);

    mgg::LinuxVirtualTerminal vt(fops, mt::fake_shared(mock_report));

    auto succeeding_handler = [] { return true; };
    auto failing_handler = [] { return false; };
    vt.register_switch_handlers(mock_main_loop, succeeding_handler, failing_handler);

    /* Fake a VT switch away request */
    sig_handler(SIGUSR1);
    /* Fake a VT switch back request */
    sig_handler(SIGUSR1);
}
