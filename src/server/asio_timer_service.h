/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_ASIO_TIMER_SERVICE_H_
#define MIR_ASIO_TIMER_SERVICE_H_

#include "mir/time/timer_service.h"
#include "asio_server_action_queue.h"

#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include <memory>
#include <thread>

namespace mir
{


class AsioTimerService : public time::TimerService
{
public:
    class AlarmImpl;
    explicit AsioTimerService(std::shared_ptr<time::Clock> const& clock);
    ~AsioTimerService() noexcept(true);

    void run() override;
    void stop() override;

    std::unique_ptr<time::Alarm> notify_in(std::chrono::milliseconds delay,
                                           std::function<void()> callback) override;
    std::unique_ptr<time::Alarm> notify_at(mir::time::Timestamp time_point,
                                           std::function<void()> callback) override;

private:
    void reschedule_alarm(AlarmImpl &);
    boost::asio::io_service io;
    boost::asio::io_service::work work;

    std::shared_ptr<time::Clock> const clock;
    boost::optional<std::thread::id> timer_thread;
    mir::AsioServerActionQueue action_queue;
    friend class AlarmImpl;
};

}

#endif
