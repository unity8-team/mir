/*
 * Copyright © 2015 Canonical Ltd.
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
 * Author: Brandon Schaefer <brandon.schaefer@canonical.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "boost/throw_exception.hpp"

#include <stdio.h>
#include <sys/wait.h>

#include "mir/fill_vector_with_random.h"

namespace
{
    /* Using rngtest we only want to check 1 20,000bit block and then exit */
    char const* COMMAND   = "rngtest -c 1";
    /* Extra 4 since rngtest eats the first 32bits */
    int const BUFFER_SIZE = 2504;
    int const MAX_TRIES   = 3;
}

TEST(FillVectorRandomTest, fill_vector_random_test)
{
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    FILE* command_file_ptr = NULL;
    int status = 0;

    /* Because randomness is generally random, we try a few times to reduce the
       risk of false positives
    */
    for (int tries = 0; tries < MAX_TRIES; tries++)
    {
        if ((command_file_ptr = popen(COMMAND, "w")) == NULL)
            BOOST_THROW_EXCEPTION(std::runtime_error(std::string("Failed to popen command: " +
                                                                 std::string(COMMAND) +
                                                                 ". Check rng-tools is installed.")));

        mir::fill_vector_with_random_data(buffer);

        fwrite(buffer.data(), sizeof(uint8_t), buffer.size(), command_file_ptr);

        int ret = pclose(command_file_ptr);
        if(WIFEXITED(ret))
            status += WEXITSTATUS(ret);
    }

    EXPECT_LE(status, MAX_TRIES - 1);
}
