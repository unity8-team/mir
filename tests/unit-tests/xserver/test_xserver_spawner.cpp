/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include <future>
#include <vector>
#include <thread>
#include <condition_variable>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mir/process/spawner.h"

#include "src/server/xserver/global_socket_listening_server_spawner.h"

using namespace ::testing;

namespace
{
struct MockProcessSpawner : public mir::process::Spawner
{
    MOCK_CONST_METHOD3(run,
                       std::shared_ptr<mir::process::Handle>(std::string, std::vector<char const*>, std::vector<int>));

    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary) const override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, std::initializer_list<char const*>(), std::initializer_list<int>()));
        return dummy_promise.get_future();
    }
    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary,
                                                                     std::initializer_list<char const*> args) const
        override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, args, std::initializer_list<int>()));
        return dummy_promise.get_future();
    }

    std::future<std::shared_ptr<mir::process::Handle>> run_from_path(char const* binary,
                                                                     std::initializer_list<char const*> args,
                                                                     std::initializer_list<int> fds) const override
    {
        std::promise<std::shared_ptr<mir::process::Handle>> dummy_promise;
        dummy_promise.set_value(run(binary, args, fds));
        return dummy_promise.get_future();
    }
};

struct SocketListeningServerTest : public testing::Test
{
    SocketListeningServerTest()
        : default_server_number("100"),
          spawner(std::make_shared<NiceMock<MockProcessSpawner>>())
    {
        ON_CALL(*spawner, run(_, _, _))
            .WillByDefault(DoAll(SaveArg<0>(&binary),
                                 SaveArg<1>(&args),
                                 SaveArg<2>(&fds),
                                 InvokeWithoutArgs([this]() { write_server_string(default_server_number); }),
                                 Return(std::shared_ptr<mir::process::Handle>())));
    }

    void write_server_string(std::string server_number)
    {
        auto location = std::find_if(args.begin(), args.end(), [](char const* a)
                                     { return strcmp(a, "-displayfd") == 0; });
        ASSERT_NE(location, args.end());
        ASSERT_NE(++location, args.end());
        int server_fd = atoi(*location);
        write(server_fd, server_number.data(), server_number.length());
        close(server_fd);
    }

    std::string default_server_number;
    std::string binary;
    std::vector<char const*> args;
    std::vector<int> fds;
    std::shared_ptr<NiceMock<MockProcessSpawner>> spawner;
};
}

TEST_F(SocketListeningServerTest, CreateServerAlwaysValid)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server(spawner);
    ASSERT_NE(server_context.get(), nullptr);
}

TEST_F(SocketListeningServerTest, SpawnsCorrectExecutable)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server(spawner);
    server_context.get();

    EXPECT_EQ(binary, "Xorg");
}

namespace
{
MATCHER_P(ContainsSubsequence, subsequence, "")
{
    auto location =
        std::search(arg.begin(), arg.end(), subsequence.begin(), subsequence.end(), [](char const* a, std::string b)
                    { return strcmp(a, b.c_str()) == 0; });
    return location != arg.end();
}
}

TEST_F(SocketListeningServerTest, SpawnsWithDisplayFDSet)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    auto server_context = factory.create_server(spawner);
    server_context.get();

    ASSERT_THAT(args, Not(IsEmpty()));
    ASSERT_THAT(fds, Not(IsEmpty()));

    Matcher<std::vector<char const*>> fd_matcher = Not(_);
    for (auto fd : fds)
    {
        fd_matcher = AnyOf(fd_matcher, ContainsSubsequence(std::vector<std::string>{"-displayfd", std::to_string(fd)}));
    }
    EXPECT_THAT(args, fd_matcher);
}

TEST_F(SocketListeningServerTest, ReturnsCorrectDisplayString)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    default_server_number = "20";
    auto server_context = factory.create_server(spawner);    

    EXPECT_STREQ(":20", server_context.get()->client_connection_string());
}

TEST_F(SocketListeningServerTest, HandlesSpawnerLifecycleCorrectly)
{
    mir::X::GlobalSocketListeningServerSpawner factory;

    std::future<std::unique_ptr<mir::X::ServerContext>> server_context;

    {
        auto tmp_spawner = std::make_shared<MockProcessSpawner>();
        ON_CALL(*tmp_spawner, run(_, _, _))
            .WillByDefault(DoAll(SaveArg<0>(&binary),
                                 SaveArg<1>(&args),
                                 SaveArg<2>(&fds),
                                 InvokeWithoutArgs([this]() { write_server_string(default_server_number); }),
                                 Return(std::shared_ptr<mir::process::Handle>())));
        EXPECT_CALL(*tmp_spawner, run(_,_,_));
        server_context = factory.create_server(tmp_spawner);
    }

    ASSERT_NE(server_context.get(), nullptr);
}
