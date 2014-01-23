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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "fork_spawner.h"

#include "mir/process/handle.h"

#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <boost/throw_exception.hpp>

namespace
{
class PidHandle : public mir::process::Handle
{
public:
    PidHandle(pid_t pid)
        : child_pid(pid)
    {
    }

    Status status() const override
    {
        return Status::Running;
    }
    pid_t pid() const override
    {
        return child_pid;
    }

private:
    pid_t child_pid;
};

static std::shared_ptr<mir::process::Handle> run(char const* binary_name, std::initializer_list<char const*> args)
{
    pid_t child = fork();

    if (child == 0) {
        std::vector<char const*> argv;
        argv.push_back(binary_name);
        argv.insert(argv.end(), args);
        argv.push_back(nullptr);

        execvp(binary_name, const_cast<char* const*>(argv.data()));
    }

    // TODO: Properly sleep until the child has forked
    // This appears to be non-trivial; is there an existing library I can borrow?
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    return std::make_shared<PidHandle>(child);
}

}


std::future<std::shared_ptr<mir::process::Handle>> mir::process::ForkSpawner::run_from_path(char const* binary_name) const
{
    return std::async(std::launch::async, run, binary_name, std::initializer_list<char const*>());
}

std::future<std::shared_ptr<mir::process::Handle>> mir::process::ForkSpawner::run_from_path(char const* binary_name, std::initializer_list<char const*> args) const
{
    return std::async(std::launch::async, run, binary_name, args);
}
