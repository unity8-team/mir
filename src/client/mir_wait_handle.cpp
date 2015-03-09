/*
 * Copyright © 2012-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 *              Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir_wait_handle.h"

MirResult::MirResult()
    : error{nullptr}, surface{nullptr}, connection{nullptr}
{
}

MirWaitHandle::MirWaitHandle() :
    guard(),
    wait_condition(),
    expecting(0)
{
}

MirWaitHandle::~MirWaitHandle()
{
}

void MirWaitHandle::expect_result()
{
    std::lock_guard<std::mutex> lock(guard);

    expecting++;
}

void MirWaitHandle::result_received()
{
    result_received(MirResult());
}

void MirWaitHandle::result_received(MirResult const& r)
{
    std::lock_guard<std::mutex> lock(guard);

    received.push_back(r);
    wait_condition.notify_all();
}

void MirWaitHandle::wait_for_all()  // wait for all results you expect
{
    std::unique_lock<std::mutex> lock(guard);

    wait_condition.wait(lock, [&]{ return received.size() == expecting; });

    received.clear();
    expecting = 0;
}

void MirWaitHandle::wait_for_pending(std::chrono::milliseconds limit)
{
    std::unique_lock<std::mutex> lock(guard);

    wait_condition.wait_for(lock, limit, [&]{ return received.size() == expecting; });
}

MirResult MirWaitHandle::wait_for_one()  // wait for any single result
{
    std::unique_lock<std::mutex> lock(guard);

    wait_condition.wait(lock, [&]{ return !received.empty(); });

    auto head = received.front();
    received.pop_front();
    --expecting;

    return head;
}
