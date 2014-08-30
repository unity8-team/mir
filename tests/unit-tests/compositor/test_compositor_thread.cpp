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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#include "src/server/compositor/compositor_thread.h"

#include "mir_test_doubles/mock_compositor_loop.h"
#include "mir_test/current_thread_name.h"
#include "mir_test/signal.h"
#include "mir/raii.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <exception>
#include <csignal>
//#include <cstdlib>

namespace mc = mir::compositor;

namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{

extern "C" { typedef void (*sig_handler)(int); }
volatile sig_handler old_signal_handler = nullptr;
mt::Signal sigterm_signal;

extern "C" void signal_handler(int sig)
{
    if (sig == SIGTERM) {
        sigterm_signal.raise();
    }

    signal(sig, old_signal_handler);
}

extern "C" void ignore_signal(int sig)
{
    signal(sig, old_signal_handler);
}

class StubCompositorLoop : public mc::CompositorLoop
{
public:
    void run() override {}
    void stop() override {}
    void schedule_compositing(int) override {}
};

class CompositorThread : public testing::Test
{
public:
    CompositorThread()
    {
        new_mock_loop();
    }

    void new_mock_loop()
    {
        std::unique_ptr<mtd::MockCompositorLoop> loop{new testing::NiceMock<mtd::MockCompositorLoop>()};
        mock_loop = std::move(loop);
        ON_CALL(*mock_loop, run())
            .WillByDefault(InvokeWithoutArgs(this, &CompositorThread::raise_run_signal));
        run_signal.reset();
    }

    void raise_run_signal()
    {
        run_signal.raise();
        std::this_thread::yield();
    }

    std::unique_ptr<mtd::MockCompositorLoop> mock_loop;
    mt::Signal run_signal;
};
}

TEST_F(CompositorThread, runs_immediately_on_construction)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, run())
        .Times(AnyNumber());

    mc::CompositorThread thread{std::move(mock_loop)};

    run_signal.wait_for(std::chrono::seconds{1});
    EXPECT_TRUE(run_signal.raised());
}

TEST_F(CompositorThread, can_pause)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, stop());

    mc::CompositorThread thread{std::move(mock_loop)};

    thread.pause();

    EXPECT_FALSE(thread.is_running());
}

TEST_F(CompositorThread, only_pauses_when_thread_is_already_running)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, stop())
        .Times(1);

    mc::CompositorThread thread{std::move(mock_loop)};

    thread.pause();

    EXPECT_FALSE(thread.is_running());

    thread.pause();
}

TEST_F(CompositorThread, destructor_stops_loop)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, stop());
    mc::CompositorThread thread{std::move(mock_loop)};
}

TEST_F(CompositorThread, raises_sigterm_on_thread_exception)
{
    using namespace testing;

    sigterm_signal.reset();

    auto const raii = mir::raii::paired_calls(
        [&]{ old_signal_handler = signal(SIGTERM, signal_handler); },
        [&]{ signal(SIGTERM, old_signal_handler); });

    EXPECT_CALL(*mock_loop, run())
        .WillOnce(Throw(std::logic_error("")));

    mc::CompositorThread thread{std::move(mock_loop)};

    sigterm_signal.wait_for(std::chrono::seconds{1});
    EXPECT_TRUE(sigterm_signal.raised());
}

TEST_F(CompositorThread, pause_does_not_block_after_thread_exception)
{
    using namespace testing;

    auto const raii = mir::raii::paired_calls(
        [&]{ old_signal_handler = signal(SIGTERM, ignore_signal); },
        [&]{ signal(SIGTERM, old_signal_handler); });

    auto throw_on_run = [this] {
        run_signal.raise();
        throw std::runtime_error("");
    };
    EXPECT_CALL(*mock_loop, run())
        .WillOnce(Invoke(throw_on_run));

    mc::CompositorThread thread{std::move(mock_loop)};

    run_signal.wait_for(std::chrono::seconds{1});
    EXPECT_TRUE(run_signal.raised());

    thread.pause();
}

TEST_F(CompositorThread, names_itself)
{
    using namespace testing;

    std::string thread_name;
    auto check_thread_name = [&]{
        thread_name = mt::current_thread_name();
        run_signal.raise();
    };

    EXPECT_CALL(*mock_loop, run())
        .WillRepeatedly(InvokeWithoutArgs(check_thread_name));

    mc::CompositorThread thread{std::move(mock_loop)};

    run_signal.wait_for(std::chrono::seconds{1});

    EXPECT_TRUE(run_signal.raised());
    EXPECT_THAT(thread_name, Eq("Mir/Comp"));
}

TEST_F(CompositorThread, makes_loop_schedule_compositing)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, schedule_compositing(_));

    mc::CompositorThread thread{std::move(mock_loop)};
    int const arbitrary_num_frames = 1;
    thread.schedule_compositing(arbitrary_num_frames);
}

TEST_F(CompositorThread, throws_when_run_invoked_before_pause)
{
    using namespace testing;

    mc::CompositorThread thread{std::move(mock_loop)};

    new_mock_loop();

    EXPECT_THROW(thread.run(std::move(mock_loop)), std::logic_error);
}

TEST_F(CompositorThread, can_run_another_loop)
{
    using namespace testing;

    EXPECT_CALL(*mock_loop, run())
        .Times(AnyNumber());

    mc::CompositorThread thread{std::move(mock_loop)};

    run_signal.wait_for(std::chrono::seconds{1});
    EXPECT_TRUE(run_signal.raised());

    new_mock_loop();

    EXPECT_FALSE(run_signal.raised());

    EXPECT_CALL(*mock_loop, run())
        .Times(AnyNumber());

    thread.pause();
    thread.run(std::move(mock_loop));

    run_signal.wait_for(std::chrono::seconds{1});
    EXPECT_TRUE(run_signal.raised());
}
