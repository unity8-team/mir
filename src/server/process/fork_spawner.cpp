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

#include <unistd.h>
#include <vector>

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
}

std::shared_ptr<mir::process::Handle> mir::process::ForkSpawner::run_from_path(char const* binary_name)
{
    pid_t child = fork();

    if (child == 0) {
        execlp(binary_name, binary_name, static_cast<char *>(NULL));
    }
    return std::make_shared<PidHandle>(child);
}

std::shared_ptr<mir::process::Handle> mir::process::ForkSpawner::run_from_path(char const* binary_name, std::initializer_list<char const*> args)

{
    pid_t child = fork();

    if (child == 0) {
        std::vector<char const*> argv;
        argv.push_back(binary_name);
        argv.insert(argv.end(), args);
        argv.push_back(nullptr);

        execvp(binary_name, const_cast<char* const*>(argv.data()));
    }
    return std::make_shared<PidHandle>(child);
}
