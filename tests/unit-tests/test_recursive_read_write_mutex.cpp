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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/recursive_read_write_mutex.h"

#include "mir_test/barrier.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mt = mir::test;

using namespace testing;

namespace
{
/* These tests are not ideal as they may fail by hanging. But I don't know a good
 * way to kill a thread within the process (i.e. without leaking resources and
 * causing "undefined behavior"). That is having a "watchdog" timeout just leads
 * to other issues and running in a separate process seems OTT.
 */
struct RecursiveReadWriteMutex : public Test
{
    int const recursion_depth{1729};
    unsigned const reader_threads{42};
    mt::Barrier readonly_barrier{reader_threads};
    mt::Barrier read_and_write_barrier{reader_threads+1};
    std::vector<std::thread> threads;

    mir::RecursiveReadWriteMutex mutex;

    void SetUp()
    {
        threads.reserve(reader_threads+1);
    }

    void TearDown()
    {
        for (auto& thread : threads)
            if (thread.joinable()) thread.join();
    }

    MOCK_METHOD0(notify_read_locked, void());
    MOCK_METHOD0(notify_read_unlocking, void());
    MOCK_METHOD0(notify_write_locked, void());
    MOCK_METHOD0(notify_write_unlocking, void());

};
}

TEST_F(RecursiveReadWriteMutex, CanBeRecursivelyReadLocked)
{
    for (int i = 0; i != recursion_depth; ++i)
        mutex.read_lock();
}

TEST_F(RecursiveReadWriteMutex, CanBeRecursivelyWriteLocked)
{
    for (int i = 0; i != recursion_depth; ++i)
        mutex.write_lock();
}

TEST_F(RecursiveReadWriteMutex, CanBeWriteLockedOnThreadWithReadLock)
{
    mutex.read_lock();
    mutex.write_lock();
}

TEST_F(RecursiveReadWriteMutex, CanBeReadLockedOnThreadWithWriteLock)
{
    mutex.write_lock();
    mutex.read_lock();
}

TEST_F(RecursiveReadWriteMutex, CanBeReadLockedOnMultipleThreads)
{
    auto const reader_function =
        [&]{
            mutex.read_lock();
            notify_read_locked();

            readonly_barrier.ready();

            notify_read_unlocking();
            mutex.read_unlock();
        };

    InSequence seq;

    EXPECT_CALL(*this, notify_read_locked()).Times(reader_threads);
    EXPECT_CALL(*this, notify_read_unlocking()).Times(reader_threads);

    for (auto i = 0U; i != reader_threads; ++i)
        threads.push_back(std::thread{reader_function});
}

TEST_F(RecursiveReadWriteMutex, WriteLockWaitsForReadLocksOnOtherThreads)
{
    auto const reader_function =
        [&]{
            mutex.read_lock();
            notify_read_locked();

            read_and_write_barrier.ready();

            notify_read_unlocking();
            mutex.read_unlock();
        };

    auto const writer_function =
        [&]{
            read_and_write_barrier.ready();

            mutex.write_lock();
            notify_write_locked();
        };

    InSequence seq;

    EXPECT_CALL(*this, notify_read_locked()).Times(reader_threads);
    EXPECT_CALL(*this, notify_read_unlocking()).Times(reader_threads);
    EXPECT_CALL(*this, notify_write_locked()).Times(1);

    for (auto i = 0U; i != reader_threads; ++i)
        threads.push_back(std::thread{reader_function});

    threads.push_back(std::thread{writer_function});
}

TEST_F(RecursiveReadWriteMutex, ReadLockWaitsForWriteLocksOnOtherThreads)
{
    auto const reader_function =
        [&]{
            read_and_write_barrier.ready();

            mutex.read_lock();
            notify_read_locked();
        };

    auto const writer_function =
        [&]{
            mutex.write_lock();
            notify_write_locked();

            read_and_write_barrier.ready();

            notify_write_unlocking();
            mutex.write_unlock();
        };

    InSequence seq;

    EXPECT_CALL(*this, notify_write_locked()).Times(1);
    EXPECT_CALL(*this, notify_write_unlocking()).Times(1);
    EXPECT_CALL(*this, notify_read_locked()).Times(reader_threads);

    for (auto i = 0U; i != reader_threads; ++i)
        threads.push_back(std::thread{reader_function});

    threads.push_back(std::thread{writer_function});
}
