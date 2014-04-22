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

#include "src/server/compositor/multi_threaded_compositor.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/compositor/scene.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/null_display_buffer.h"
#include "mir_test_doubles/mock_display_buffer.h"
#include "mir_test_doubles/mock_compositor_report.h"
#include "mir_test_doubles/mock_scene.h"
#include "mir_test_doubles/stub_renderable.h"
#include "mir_test_doubles/null_display_buffer_compositor_factory.h"
#include "mir_test/spin_wait.h"

#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;
namespace mr = mir::report;

namespace
{

class StubDisplay : public mtd::NullDisplay
{
 public:
    StubDisplay(unsigned int nbuffers) : buffers{nbuffers} {}

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f) override
    {
        for (auto& db : buffers)
            f(db);
    }

private:
    std::vector<mtd::NullDisplayBuffer> buffers;
};

class StubDisplayWithMockBuffers : public mtd::NullDisplay
{
 public:
    StubDisplayWithMockBuffers(unsigned int nbuffers) : buffers{nbuffers} {}

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f)
    {
        for (auto& db : buffers)
            f(db);
    }

    void for_each_mock_buffer(std::function<void(mtd::MockDisplayBuffer&)> const& f)
    {
        for (auto& db : buffers)
            f(db);
    }

private:
    std::vector<testing::NiceMock<mtd::MockDisplayBuffer>> buffers;
};

class StubScene : public mc::Scene
{
public:
    StubScene(mg::RenderableList const& list)
        : callback{[]{}},
          throw_on_set_callback_{false}, 
          renderable_list{list}
    {
    }
    StubScene() : StubScene(mg::RenderableList{}) {}

    mg::RenderableList generate_renderable_list() const
    {
        return renderable_list;
    }

    void set_change_callback(std::function<void()> const& f)
    {
        if (throw_on_set_callback_)
            throw std::runtime_error("");
        std::lock_guard<std::mutex> lock{callback_mutex};
        assert(f);
        callback = f;
    }

    void emit_change_event()
    {
        {
            std::lock_guard<std::mutex> lock{callback_mutex};
            callback();
        }
        /* Reduce run-time under valgrind */
        std::this_thread::yield();
    }

    void throw_on_set_callback(bool flag)
    {
        throw_on_set_callback_ = flag;
    }

    void lock() {}
    void unlock() {}

private:
    std::function<void()> callback;
    std::mutex callback_mutex;
    bool throw_on_set_callback_;
    mg::RenderableList renderable_list;
};

class StubDisplayBufferCompositor : public mc::DisplayBufferCompositor
{
public:
    StubDisplayBufferCompositor(std::shared_ptr<mc::Scene> scene,
                                std::chrono::milliseconds frame_time)
        : scene{scene}, frame_time{frame_time}
    {
    }

    bool composite() override
    {
        auto const& all = scene->generate_renderable_list();
        for (auto const& r : all)
            r->buffer(this);  // Consume a frame

        std::this_thread::sleep_for(frame_time);
        return false;
    }

private:
    std::shared_ptr<mc::Scene> const scene;
    std::chrono::milliseconds const frame_time;
};


class StubDisplayBufferCompositorFactory :
    public mc::DisplayBufferCompositorFactory
{
public:
    StubDisplayBufferCompositorFactory(std::shared_ptr<mc::Scene> scene,
                                       int hz)
        : scene{scene},
          frame_time{std::chrono::milliseconds(1000 / hz)}
    {
    }

    std::unique_ptr<mc::DisplayBufferCompositor>
        create_compositor_for(mg::DisplayBuffer&) override
    {
        std::unique_ptr<mc::DisplayBufferCompositor> ret(
            new StubDisplayBufferCompositor(scene, frame_time));
        return ret;
    }

private:
    std::shared_ptr<mc::Scene> const scene;
    std::chrono::milliseconds const frame_time;
};

class RecordingDisplayBufferCompositor : public mc::DisplayBufferCompositor
{
public:
    RecordingDisplayBufferCompositor(std::function<void()> const& mark_render_buffer)
        : mark_render_buffer{mark_render_buffer}
    {
    }

    bool composite()
    {
        mark_render_buffer();
        /* Reduce run-time under valgrind */
        std::this_thread::yield();
        return false;
    }

private:
    std::function<void()> const mark_render_buffer;
};


class RecordingDisplayBufferCompositorFactory : public mc::DisplayBufferCompositorFactory
{
public:
    std::unique_ptr<mc::DisplayBufferCompositor> create_compositor_for(mg::DisplayBuffer& display_buffer)
    {
        auto raw = new RecordingDisplayBufferCompositor{
            [&display_buffer,this]()
            {
                mark_render_buffer(display_buffer);
            }};
        return std::unique_ptr<RecordingDisplayBufferCompositor>(raw);
    }

    void mark_render_buffer(mg::DisplayBuffer& display_buffer)
    {
        std::lock_guard<std::mutex> lk{m};

        if (records.find(&display_buffer) == records.end())
            records[&display_buffer] = Record(0, std::unordered_set<std::thread::id>());

        ++records[&display_buffer].first;
        records[&display_buffer].second.insert(std::this_thread::get_id());
    }

    bool enough_records_gathered(unsigned int nbuffers, unsigned int min_record_count = 1000)
    {
        std::lock_guard<std::mutex> lk{m};

        if (records.size() < nbuffers)
            return false;

        for (auto const& e : records)
        {
            Record const& r = e.second;
            if (r.first < min_record_count)
                return false;
        }

        return true;
    }

    bool each_buffer_rendered_in_single_thread()
    {
        for (auto const& e : records)
        {
            Record const& r = e.second;
            if (r.second.size() != 1)
                return false;
        }

        return true;
    }

    bool buffers_rendered_in_different_threads()
    {
        std::unordered_set<std::thread::id> seen;
        seen.insert(std::this_thread::get_id());

        for (auto const& e : records)
        {
            Record const& r = e.second;
            auto iter = r.second.begin();
            if (seen.find(*iter) != seen.end())
                return false;
            seen.insert(*iter);
        }

        return true;
    }

    bool check_record_count_for_each_buffer(
            unsigned int nbuffers,
            unsigned int min,
            unsigned int max = ~0u)
    {
        std::lock_guard<std::mutex> lk{m};

        if (records.size() < nbuffers)
            return (min == 0 && max == 0);

        for (auto const& e : records)
        {
            Record const& r = e.second;
            if (r.first < min || r.first > max)
                return false;
        }

        return true;
    }

private:
    std::mutex m;
    typedef std::pair<unsigned int, std::unordered_set<std::thread::id>> Record;
    std::unordered_map<mg::DisplayBuffer*,Record> records;
};

class SurfaceUpdatingDisplayBufferCompositor : public mc::DisplayBufferCompositor
{
public:
    SurfaceUpdatingDisplayBufferCompositor(std::function<void()> const& fake_surface_update)
        : fake_surface_update{fake_surface_update}
    {
    }

    bool composite()
    {
        fake_surface_update();
        /* Reduce run-time under valgrind */
        std::this_thread::yield();
        return false;
    }

private:
    std::function<void()> const fake_surface_update;
};

class SurfaceUpdatingDisplayBufferCompositorFactory : public mc::DisplayBufferCompositorFactory
{
public:
    SurfaceUpdatingDisplayBufferCompositorFactory(std::shared_ptr<StubScene> const& scene)
        : scene{scene},
          render_count{0}
    {
    }

    std::unique_ptr<mc::DisplayBufferCompositor> create_compositor_for(mg::DisplayBuffer&)
    {
        auto raw = new SurfaceUpdatingDisplayBufferCompositor{[this]{fake_surface_update();}};
        return std::unique_ptr<SurfaceUpdatingDisplayBufferCompositor>(raw);
    }

    void fake_surface_update()
    {
        scene->emit_change_event();
        ++render_count;
    }
    bool enough_renders_happened()
    {
        unsigned int const enough_renders{1000};
        return render_count > enough_renders;
    }

private:
    std::shared_ptr<StubScene> const scene;
    unsigned int render_count;
};

class BufferCountingRenderable : public mtd::StubRenderable
{
public:
    BufferCountingRenderable() : buffers_requested_{0} {}

    std::shared_ptr<mg::Buffer> buffer(void const*) const override
    {
        ++buffers_requested_;
        return std::make_shared<mtd::StubBuffer>();
    }

    int buffers_requested() const
    {
        return buffers_requested_;
    }

private:
    mutable std::atomic<int> buffers_requested_;
};

auto const null_report = mr::null_compositor_report();
unsigned int const composites_per_update{1};
}

TEST(MultiThreadedCompositor, compositing_happens_in_different_threads)
{
    using namespace testing;

    unsigned int const nbuffers{3};

    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<RecordingDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, true};

    compositor.start();

    while (!db_compositor_factory->enough_records_gathered(nbuffers))
        scene->emit_change_event();

    compositor.stop();

    EXPECT_TRUE(db_compositor_factory->each_buffer_rendered_in_single_thread());
    EXPECT_TRUE(db_compositor_factory->buffers_rendered_in_different_threads());
}

TEST(MultiThreadedCompositor, reports_in_the_right_places)
{
    using namespace testing;

    auto display = std::make_shared<StubDisplayWithMockBuffers>(1);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory =
        std::make_shared<RecordingDisplayBufferCompositorFactory>();
    auto mock_report = std::make_shared<mtd::MockCompositorReport>();
    mc::MultiThreadedCompositor compositor{display, scene,
                                           db_compositor_factory,
                                           mock_report,
                                           true};

    EXPECT_CALL(*mock_report, started())
        .Times(1);

    display->for_each_mock_buffer([](mtd::MockDisplayBuffer& mock_buf)
    {
        EXPECT_CALL(mock_buf, make_current()).Times(1);
        EXPECT_CALL(mock_buf, view_area())
            .WillOnce(Return(geom::Rectangle()));
    });

    EXPECT_CALL(*mock_report, added_display(_,_,_,_,_))
        .Times(1);
    EXPECT_CALL(*mock_report, scheduled())
        .Times(2);

    display->for_each_mock_buffer([](mtd::MockDisplayBuffer& mock_buf)
    {
        EXPECT_CALL(mock_buf, release_current()).Times(1);
    });

    EXPECT_CALL(*mock_report, stopped())
        .Times(AtLeast(1));

    compositor.start();
    scene->emit_change_event();
    while (!db_compositor_factory->check_record_count_for_each_buffer(1, composites_per_update))
        std::this_thread::yield();
    compositor.stop();
}

/*
 * It's difficult to test that a render won't happen, without some further
 * introspective capabilities that would complicate the code. This test will
 * catch a problem if it occurs, but can't ensure that a problem, even if
 * present, will occur in a determinstic manner.
 *
 * Nonetheless, the test is useful since it's very likely to fail if a problem
 * is present (and don't forget that it's usually run multiple times per day).
 */
TEST(MultiThreadedCompositor, composites_only_on_demand)
{
    using namespace testing;

    unsigned int const nbuffers = 3;

    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<RecordingDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, true};

    // Verify we're actually starting at zero frames
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 0, 0));

    compositor.start();

    // Initial render: initial_composites frames should be composited at least
    while (!db_compositor_factory->check_record_count_for_each_buffer(nbuffers, composites_per_update))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Now we have initial_composites redraws, pause for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // ... and make sure the number is still only 3
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, composites_per_update, composites_per_update));

    // Trigger more surface changes
    scene->emit_change_event();

    // Display buffers should be forced to render another 3, so that's 6
    while (!db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 2*composites_per_update))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Now pause without any further surface changes
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify we never triggered more than 2*initial_composites compositions
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 2*composites_per_update, 2*composites_per_update));

    compositor.stop();  // Pause the compositor so we don't race

    // Now trigger many surfaces changes close together
    for (int i = 0; i < 10; i++)
        scene->emit_change_event();

    compositor.start();

    // Display buffers should be forced to render another 3, so that's 9
    while (!db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 3*composites_per_update))
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Now pause without any further surface changes
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify we never triggered more than 9 compositions
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 3*composites_per_update, 3*composites_per_update));

    compositor.stop();
}

TEST(MultiThreadedCompositor, when_no_initial_composite_is_needed_there_is_none)
{
    using namespace testing;

    unsigned int const nbuffers = 3;

    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<RecordingDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, false};

    // Verify we're actually starting at zero frames
    ASSERT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 0, 0));

    compositor.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify we're still at zero frames
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 0, 0));

    compositor.stop();
}

TEST(MultiThreadedCompositor, when_no_initial_composite_is_needed_we_still_composite_on_restart)
{
    using namespace testing;

    unsigned int const nbuffers = 3;

    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<RecordingDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, false};

    compositor.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify we're actually starting at zero frames
    ASSERT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, 0, 0));

    compositor.stop();
    compositor.start();

    for (int countdown = 100;
        countdown != 0 &&
        !db_compositor_factory->check_record_count_for_each_buffer(nbuffers, composites_per_update, composites_per_update);
        --countdown)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify we composited the expected frame
    EXPECT_TRUE(db_compositor_factory->check_record_count_for_each_buffer(nbuffers, composites_per_update, composites_per_update));

    compositor.stop();
}

TEST(MultiThreadedCompositor, surface_update_from_render_doesnt_deadlock)
{
    using namespace testing;

    unsigned int const nbuffers{3};

    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<SurfaceUpdatingDisplayBufferCompositorFactory>(scene);
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, true};

    compositor.start();

    while (!db_compositor_factory->enough_renders_happened())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    compositor.stop();
}

TEST(MultiThreadedCompositor, makes_and_releases_display_buffer_current_target)
{
    using namespace testing;

    unsigned int const nbuffers{3};

    auto display = std::make_shared<StubDisplayWithMockBuffers>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<mtd::NullDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, true};

    display->for_each_mock_buffer([](mtd::MockDisplayBuffer& mock_buf)
    {
        EXPECT_CALL(mock_buf, view_area())
            .WillOnce(Return(geom::Rectangle()));
        EXPECT_CALL(mock_buf, make_current()).Times(1);
        EXPECT_CALL(mock_buf, release_current()).Times(1);
    });

    compositor.start();
    compositor.stop();
}

TEST(MultiThreadedCompositor, double_start_or_stop_ignored)
{
    using namespace testing;

    unsigned int const nbuffers{3};
    auto display = std::make_shared<StubDisplayWithMockBuffers>(nbuffers);
    auto mock_scene = std::make_shared<mtd::MockScene>();
    auto db_compositor_factory = std::make_shared<mtd::NullDisplayBufferCompositorFactory>();
    auto mock_report = std::make_shared<testing::NiceMock<mtd::MockCompositorReport>>();
    EXPECT_CALL(*mock_report, started())
        .Times(1);
    EXPECT_CALL(*mock_report, stopped())
        .Times(1);
    EXPECT_CALL(*mock_scene, set_change_callback(_))
        .Times(2);
    EXPECT_CALL(*mock_scene, generate_renderable_list())
        .Times(AtLeast(0))
        .WillRepeatedly(Return(mg::RenderableList{}));

    mc::MultiThreadedCompositor compositor{display, mock_scene, db_compositor_factory, mock_report, true};

    compositor.start();
    compositor.start();
    compositor.stop();
    compositor.stop();
}

TEST(MultiThreadedCompositor, consumes_buffers_for_renderables_that_are_not_rendered)
{
    using namespace testing;

    auto renderable = std::make_shared<BufferCountingRenderable>();

    // Defining zero outputs is the simplest way to emulate what would
    // happen if all your displays were blocked in swapping...
    auto display = std::make_shared<StubDisplay>(0);
    auto stub_scene = std::make_shared<StubScene>(mg::RenderableList{renderable});
    auto db_compositor_factory = std::make_shared<mtd::NullDisplayBufferCompositorFactory>();

    mc::MultiThreadedCompositor compositor{
        display, stub_scene, db_compositor_factory, null_report, true};

    compositor.start();

    mir::test::spin_wait_for_condition_or_timeout(
        [&] { return renderable->buffers_requested() == 1; },
        std::chrono::seconds{5});

    EXPECT_THAT(renderable->buffers_requested(), Eq(1));

    stub_scene->emit_change_event();

    mir::test::spin_wait_for_condition_or_timeout(
        [&] { return renderable->buffers_requested() == 2; },
        std::chrono::seconds{5});

    EXPECT_THAT(renderable->buffers_requested(), Eq(2));

    compositor.stop();
}

TEST(MultiThreadedCompositor, never_steals_frames_from_real_displays)
{
    /*
     * Verify dummy frames are consumed slower than any physical display would
     * consume them, so that the two types of consumers don't race and don't
     * visibly skip (LP: #1308843)
     */
    using namespace testing;

    unsigned int const nbuffers{2};
    auto renderable = std::make_shared<BufferCountingRenderable>();
    auto display = std::make_shared<StubDisplay>(nbuffers);
    auto stub_scene = std::make_shared<StubScene>(mg::RenderableList{renderable});
    // We use NullDisplayBufferCompositors to simulate DisplayBufferCompositors
    // not rendering a renderable.
    auto db_compositor_factory = std::make_shared<mtd::NullDisplayBufferCompositorFactory>();

    mc::MultiThreadedCompositor compositor{
        display, stub_scene, db_compositor_factory, null_report, true};

    compositor.start();

    // Realistically I've only ever seen LCDs go as low as 40Hz. But some TV
    // outputs will go as low as 24Hz (traditional movie frame rate). So we
    // need to ensure the dummy consumer is slower than that...
    int const min_real_refresh_rate = 20;

    int const secs = 5;
    auto const duration = std::chrono::seconds{secs};
    mir::test::spin_wait_for_condition_or_timeout(
        [&] { return renderable->buffers_requested() <= 1; },
        duration);

    auto const end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end)
    {
        stub_scene->emit_change_event();
        std::this_thread::yield();
    }

    int const max_dummy_frames = min_real_refresh_rate * secs;
    ASSERT_GT(max_dummy_frames, renderable->buffers_requested());

    compositor.stop();
}

TEST(MultiThreadedCompositor, only_real_displays_limit_consumption_rate)
{
    using namespace testing;

    auto display = std::make_shared<StubDisplay>(1);
    auto renderable = std::make_shared<BufferCountingRenderable>();
    auto stub_scene = std::make_shared<StubScene>(mg::RenderableList{renderable});

    int const framerate = 75;   // Simulate a nice fast frame rate
    auto db_compositor_factory =
        std::make_shared<StubDisplayBufferCompositorFactory>(stub_scene,
                                                             framerate);

    mc::MultiThreadedCompositor compositor{
        display, stub_scene, db_compositor_factory, null_report, false};

    compositor.start();

    int const secs = 5;
    auto const duration = std::chrono::seconds{secs};
    auto const end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end)
    {
        stub_scene->emit_change_event();
        std::this_thread::yield();
    }

    int const expected_frames = framerate * secs;
    ASSERT_GT(expected_frames * 1.2f, renderable->buffers_requested());
    ASSERT_LT(expected_frames * 0.8f, renderable->buffers_requested());

    compositor.stop();
}

TEST(MultiThreadedCompositor, cleans_up_after_throw_in_start)
{
    unsigned int const nbuffers{3};

    auto display = std::make_shared<StubDisplayWithMockBuffers>(nbuffers);
    auto scene = std::make_shared<StubScene>();
    auto db_compositor_factory = std::make_shared<RecordingDisplayBufferCompositorFactory>();
    mc::MultiThreadedCompositor compositor{display, scene, db_compositor_factory, null_report, true};

    scene->throw_on_set_callback(true);

    EXPECT_THROW(compositor.start(), std::runtime_error);

    scene->throw_on_set_callback(false);

    /* No point in running the rest of the test if it throws again */
    ASSERT_NO_THROW(compositor.start());

    /* The minimum number of records here should be nbuffers *2, since we are checking for
     * presence of at least one additional rogue compositor thread per display buffer
     * However to avoid timing considerations like one good thread compositing the display buffer
     * twice before the rogue thread gets a chance to, an arbitrary number of records are gathered
     */
    unsigned int min_number_of_records = 100;

    /* Timeout here in case the exception from setting the scene callback put the compositor
     * in a bad state that did not allow it to composite (hence no records gathered)
     */
    auto time_out = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!db_compositor_factory->enough_records_gathered(nbuffers, min_number_of_records) &&
           std::chrono::steady_clock::now() <= time_out)
    {
        scene->emit_change_event();
        std::this_thread::yield();
    }

    /* Check expectation in case a timeout happened */
    EXPECT_TRUE(db_compositor_factory->enough_records_gathered(nbuffers, min_number_of_records));

    compositor.stop();

    /* Only one thread should be rendering each display buffer
     * If the compositor failed to cleanup correctly more than one thread could be
     * compositing the same display buffer
     */
    EXPECT_TRUE(db_compositor_factory->each_buffer_rendered_in_single_thread());
}
