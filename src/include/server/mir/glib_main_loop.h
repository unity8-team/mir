#ifndef MIR_GLIB_MAIN_LOOP_H_
#define MIR_GLIB_MAIN_LOOP_H_

#include "mir/main_loop.h"

#include <glib.h>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_set>

namespace mir
{
namespace time
{
class Clock;
}
namespace detail
{
struct SignalGSource;
struct FdGSource;
struct TimerGSource;
struct ServerActionGSource;
}

class GLibMainLoop : public MainLoop
{
public:
    explicit GLibMainLoop(std::shared_ptr<time::Clock> const& clock);
    ~GLibMainLoop();

    void run() override;
    void stop() override;

    void register_signal_handler(
        std::initializer_list<int> signals,
        std::function<void(int)> const& handler) override;

    void register_fd_handler(
        std::initializer_list<int> fds,
        void const* owner,
        std::function<void(int)> const& handler) override;

    void unregister_fd_handler(
        void const* owner) override;

    std::unique_ptr<mir::time::Alarm> notify_in(
        std::chrono::milliseconds delay,
        std::function<void()> callback) override;

    std::unique_ptr<mir::time::Alarm> notify_at(
        mir::time::Timestamp t,
        std::function<void()> callback) override;

    std::unique_ptr<time::Alarm> create_alarm(std::function<void()> callback) override;

    void enqueue(void const* owner, ServerAction const& action) override;
    void pause_processing_for(void const* owner) override;
    void resume_processing_for(void const* owner) override;

    void flush();

private:
    std::shared_ptr<time::Clock> const clock;
    std::shared_ptr<GMainContext> main_context;
    std::atomic<bool> running;

    std::mutex fd_gsources_mutex;
    std::vector<std::shared_ptr<detail::FdGSource>> fd_gsources;

    std::mutex signal_gsource_mutex;
    std::shared_ptr<detail::SignalGSource> signal_gsource;

    std::mutex before_next_iteration_mutex;
    std::function<void()> before_next_iteration;

    std::mutex server_actions_mutex;
    std::unordered_set<void const*> do_not_process;
};
        
}

#endif
