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

#include "src/server/process/fork_spawner.cpp"

#include <set>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;

TEST(ForkSpawnerTest, OpenFDsListsCorrectFDs)
{
    // We start with stdin, stdout stderr
    std::set<int> std_fds{0, 1, 2};
    std::set<int> extra_fds;

    extra_fds.insert(open("/dev/null", O_RDONLY));

    // Ensure we have a hole in our fd numbering
    int spacing_fd = open("/dev/null", O_RDONLY);

    extra_fds.insert(open("/dev/null", O_RDONLY));
    extra_fds.insert(open("/dev/null", O_RDONLY));

    close(spacing_fd);

    for(auto fd : open_fds())
    {
        if (std_fds.erase(fd) != 1)
        {
            if (extra_fds.erase(fd) != 1)
                FAIL() << "Unexpected fd found: " << fd;

            close(fd);
        }
    }

    EXPECT_THAT(std_fds, IsEmpty());
    EXPECT_THAT(extra_fds, IsEmpty());

    for(auto fd : extra_fds)
    {
        close(fd);
    }
}
