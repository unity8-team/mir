/*
 * Copyright © 2014-2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#ifndef MIR_GLIB_MAIN_LOOP_H_
#define MIR_GLIB_MAIN_LOOP_H_

#include "mir/main_loop.h"
#include "mir/glib_main_loop_sources.h"

#include <atomic>
#include <vector>
#include <mutex>
#include <exception>

#include <glib.h>

namespace mir
{

namespace detail
{

class GMainContextHandle
{
public:
    GMainContextHandle();
    ~GMainContextHandle();
    operator GMainContext*() const;
private:
    GMainContext* const main_context;
};

}

class GLibMainLoop : public MainLoop
{
public:
    GLibMainLoop(std::shared_ptr<time::Clock> const& clock);

    void run() override;
    void stop() override;

    void register_signal_handler(
        std::initializer_list<int> signals,
        std::function<void(int)> const& handler) override;

    void register_fd_handler(
        std::initializer_list<int> fds,
        void const* owner,
        std::function<void(int)> const& handler) override;

    void unregister_fd_handler(void const* owner) override;

    void enqueue(void const* owner, ServerAction const& action) override;
    void pause_processing_for(void const* owner) override;
    void resume_processing_for(void const* owner) override;

    std::unique_ptr<mir::time::Alarm> notify_in(
        std::chrono::milliseconds delay,
        std::function<void()> const& callback) override;

    std::unique_ptr<mir::time::Alarm> notify_at(
        mir::time::Timestamp t,
        std::function<void()> const& callback) override;

    std::unique_ptr<mir::time::Alarm> create_alarm(
        std::function<void()> const& callback) override;

    std::unique_ptr<mir::time::Alarm> create_alarm(
        std::function<void()> const& callback,
        std::function<void()> const& lock,
        std::function<void()> const& unlock) override;

    void reprocess_all_sources();

private:
    bool should_process_actions_for(void const* owner);
    void handle_exception(std::exception_ptr const& e);

    std::shared_ptr<time::Clock> const clock;
    detail::GMainContextHandle const main_context;
    std::atomic<bool> running;
    detail::FdSources fd_sources;
    detail::SignalSources signal_sources;
    std::mutex do_not_process_mutex;
    std::vector<void const*> do_not_process;
    std::function<void()> before_iteration_hook;
    std::exception_ptr main_loop_exception;
};

}

#endif
