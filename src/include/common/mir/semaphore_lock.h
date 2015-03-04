/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MIR_SEMAPHORE_LOCK_H_
#define MIR_SEMAPHORE_LOCK_H_

#include <boost/throw_exception.hpp>

#include <exception>
#include <mutex>
#include <condition_variable>

namespace mir
{

class SemaphoreLock
{
public:
    // Make it a BasicLockable so it can be used with
    // std::lock_guard or std::unique_lock
    void lock()
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        cv.wait(lock, [this]{ return !locked;});
        locked = true;
    }

    void unlock()
    {
        std::lock_guard<decltype(mutex)> lock{mutex};

        if (!locked)
            BOOST_THROW_EXCEPTION(std::logic_error("Already unlocked"));

        locked = false;
        cv.notify_one();
    }

private:
    std::mutex mutex;
    std::condition_variable cv;
    bool locked = false;
};

class SemaphoreUnlockIfUnwinding
{
public:
    explicit SemaphoreUnlockIfUnwinding(SemaphoreLock& guard)
        : guard(guard)
    {
        guard.lock();
    }

    ~SemaphoreUnlockIfUnwinding()
    {
        if (std::uncaught_exception())
            guard.unlock();
    }

private:
    SemaphoreLock& guard;
};

}

#endif /* MIR_SEMAPHORE_LOCK_H_ */
