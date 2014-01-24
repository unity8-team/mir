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
#include "mir/pipe.h"
#include "mir/raii.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <vector>
#include <thread>
#include <string>
#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

namespace
{
class PidHandle : public mir::process::Handle
{
public:
    PidHandle(pid_t pid) : child_pid(pid)
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

static std::vector<int> open_fds()
{
    std::vector<int> fds;

    DIR* process_fds_dir;
    auto const dir_raii = mir::raii::paired_calls([&process_fds_dir]
                                                  { process_fds_dir = opendir("/proc/self/fd"); },
                                                  [&process_fds_dir]
                                                  {
                                                      if (process_fds_dir != nullptr)
                                                          closedir(process_fds_dir);
                                                  });

    if (process_fds_dir == nullptr)
        BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failed to open process fds directory"))
                              << boost::errinfo_errno(errno));

    struct dirent* entry;
    errno = 0;
    while ((entry = readdir(process_fds_dir)) != nullptr)
    {
        // We don't care about directory links
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        int fd;
        char *conversion_end;
        fd = strtol(entry->d_name, &conversion_end, 10);

        if (*conversion_end != '\0' || conversion_end == entry->d_name)
            BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Unexpected fd name: ") + entry->d_name));

        if (fd != dirfd(process_fds_dir))
            fds.push_back(fd);
    }
    if (errno != 0)
        BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failed to read from process fds directory"))
                              << boost::errinfo_errno(errno));

    return fds;
}

static std::shared_ptr<mir::process::Handle> run(char const* binary_name, std::initializer_list<char const*> args)
{
    mir::pipe::Pipe error_pipe(O_CLOEXEC);
    pid_t child = fork();

    if (child == 0)
    {
        error_pipe.close_read_fd();

        for(auto fd : open_fds())
        {
            // We don't want to close stdin, stdout, stderr, or our cross-process pipe
            if (fd >= 3 && fd != error_pipe.write_fd())
                close (fd);
        }

        std::vector<char const*> argv;
        argv.push_back(binary_name);
        argv.insert(argv.end(), args);
        argv.push_back(nullptr);

        execvp(binary_name, const_cast<char* const*>(argv.data()));
        // We can only get here by failing to exec
        write(error_pipe.write_fd(), &errno, sizeof errno);
    }

    error_pipe.close_write_fd();
    error_t error = 0;
    if (read(error_pipe.read_fd(), &error, sizeof error) == -1)
        BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failed to read from pipe"))
                              << boost::errinfo_errno(errno));

    if (error != 0)
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(std::runtime_error(std::string("Failed to execute process: ") + binary_name))
            << boost::errinfo_errno(errno));

    return std::make_shared<PidHandle>(child);
}
}

std::future<std::shared_ptr<mir::process::Handle>> mir::process::ForkSpawner::run_from_path(char const* binary_name)
    const
{
    return std::async(std::launch::async, run, binary_name, std::initializer_list<char const*>());
}

std::future<std::shared_ptr<mir::process::Handle>> mir::process::ForkSpawner::run_from_path(
    char const* binary_name, std::initializer_list<char const*> args) const
{
    return std::async(std::launch::async, run, binary_name, args);
}
