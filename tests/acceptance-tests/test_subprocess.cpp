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

#include "src/server/process/fork_spawner.h"
#include "mir/process/spawner.h"
#include "mir/process/handle.h"

#include <fstream>
#include <sstream>
#include <unistd.h>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

TEST(ProcessTest, RunFromPathRunsCorrectBinary)
{
    mir::process::ForkSpawner spawner;

    auto future_handle = spawner.run_from_path("true");
    auto handle = future_handle.get();

    std::stringstream stat_path;
    stat_path<<"/proc/"<<handle->pid()<<"/stat";

    std::ifstream status{stat_path.str()};

    char binary_name[PATH_MAX];

    // Read up to the initial '('
    while (status.good() && !status.eof() && (status.get() != '('));

    status.getline(binary_name, sizeof(binary_name), ')');

    EXPECT_STREQ("true", binary_name);
}

TEST(ProcessTest, ChildHasExpectedFDs)
{
    mir::process::ForkSpawner spawner;

    auto fd = open("/dev/null", O_RDONLY);
    auto future_handle = spawner.run_from_path("sleep", {"1"});
    auto handle = future_handle.get();

    // TODO: Why is this racy?
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    std::stringstream fds_path;
    fds_path<<"/proc/"<<handle->pid()<<"/fd";

    bool found_stdin = false, found_stdout = false, found_stderr = false;

    DIR* process_fds_dir = opendir(fds_path.str().c_str());
    ASSERT_NE(process_fds_dir, nullptr)
        <<"Error opening "<<fds_path.str()<<": "<<strerror(errno)<<" ("<<errno<<")"<< std::endl;
    struct dirent* entry;
    while ((entry = readdir(process_fds_dir)) != nullptr)
    {
        // We don't care about directory links
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (strcmp(entry->d_name, "0") == 0)
        {
            found_stdin = true;
        }
        else if (strcmp(entry->d_name, "1") == 0)
        {
            found_stdout = true;
        }
        else if (strcmp(entry->d_name, "2") == 0)
        {
            found_stderr = true;
        }
        else
        {
            ADD_FAILURE() << "Unexpected fd: " << entry->d_name << std::endl;
        }
    }
    EXPECT_TRUE(found_stdin);
    EXPECT_TRUE(found_stdout);
    EXPECT_TRUE(found_stderr);
    closedir(process_fds_dir);
    close(fd);
}

TEST(ProcessTest, ChildReceivesExpectedCmdline)
{
    mir::process::ForkSpawner spawner;

    auto future_handle = spawner.run_from_path("sleep", {"10"});
    auto handle = future_handle.get();

    std::stringstream cmdline_path;
    cmdline_path<<"/proc/"<<handle->pid()<<"/cmdline";

    std::ifstream cmdline{cmdline_path.str()};

    // We expect "sleep\010\0\0"
    char buffer[5 + 1 + 2 + 1 + 1];
    cmdline.read(buffer, sizeof(buffer));

    EXPECT_STREQ("sleep", buffer);
    EXPECT_STREQ("10", buffer + 6);
}

TEST(ProcessTest, SpawningNonExistentBinaryThrows)
{
    mir::process::ForkSpawner spawner;

    auto future_handle = spawner.run_from_path("I'm a binary that almost certainly doesn't exist");

    EXPECT_THROW(future_handle.get(), std::runtime_error);
}
