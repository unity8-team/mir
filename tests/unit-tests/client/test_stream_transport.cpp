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

#include "src/client/rpc/stream_transport.h"
#include "src/client/rpc/stream_socket_transport.h"

#include "mir_test/auto_unblock_thread.h"
#include "mir_test/signal.h"
#include "mir/raii.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <cstdint>
#include <thread>
#include <system_error>
#include <array>
#include <atomic>
#include <array>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mclr = mir::client::rpc;

namespace
{
class MockObserver : public mclr::StreamTransport::Observer
{
public:
    MOCK_METHOD0(on_data_available, void());
    MOCK_METHOD0(on_disconnected, void());
};
}

template<typename TransportMechanism>
class StreamTransportTest : public ::testing::Test
{
public:
    StreamTransportTest()
    {
        int socket_fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_fds) < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        test_fd = socket_fds[0];
        transport_fd = socket_fds[1];
        transport = std::make_shared<TransportMechanism>(socket_fds[1]);
    }

    virtual ~StreamTransportTest()
    {
        // We don't care about errors, so unconditionally close the test fd.
        close(test_fd);
    }

    int transport_fd;
    int test_fd;
    std::shared_ptr<TransportMechanism> transport;
};

typedef ::testing::Types<mclr::StreamSocketTransport> Transports;
TYPED_TEST_CASE(StreamTransportTest, Transports);

TYPED_TEST(StreamTransportTest, NoticesRemoteDisconnect)
{
    using namespace testing;
    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    ON_CALL(*observer, on_disconnected())
        .WillByDefault(Invoke([done]() { done->raise(); }));

    this->transport->register_observer(observer);

    close(this->test_fd);

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, NoticesRemoteDisconnectWhileReadingInIOLoop)
{
    using namespace testing;
    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();
    bool data_notified{false};
    bool finished_read{false};

    ON_CALL(*observer, on_disconnected())
        .WillByDefault(Invoke([done]() { done->raise(); }));
    ON_CALL(*observer, on_data_available())
            .WillByDefault(Invoke([this, &data_notified, &finished_read]()
    {
        data_notified = true;
        char buffer[8];
        this->transport->receive_data(buffer, sizeof(buffer));
        finished_read = true;
    }));

    this->transport->register_observer(observer);

    uint32_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    close(this->test_fd);

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{1}));
    EXPECT_TRUE(data_notified);
    EXPECT_FALSE(finished_read);
}

TYPED_TEST(StreamTransportTest, NotifiesOnDataAvailable)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([done]() { done->raise(); }));

    this->transport->register_observer(observer);

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, DoesntNotifyUntilDataAvailable)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([done]() { done->raise(); }));

    this->transport->register_observer(observer);

    std::this_thread::sleep_for(std::chrono::seconds{1});

    EXPECT_FALSE(done->raised());

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, KeepsNotifyingOfAvailableDataUntilAllIsRead)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    std::array<uint8_t, sizeof(int) * 256> data;
    data.fill(0);
    std::atomic<size_t> bytes_left{data.size()};

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([done, &bytes_left, this]()
    {
        int dummy;
        this->transport->receive_data(&dummy, sizeof(dummy));
        bytes_left.fetch_sub(sizeof(dummy));
        if (bytes_left.load() == 0)
        {
            done->raise();
        }
    }));

    this->transport->register_observer(observer);

    EXPECT_EQ(data.size(), write(this->test_fd, data.data(), data.size()));

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{5}));
    EXPECT_EQ(0, bytes_left.load());
}

TYPED_TEST(StreamTransportTest, StopsNotifyingOnceAllDataIsRead)
{
    using namespace testing;
    int const buffer_size{256};

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([this, done]()
    {
        if (done->raised())
        {
            FAIL() << "on_data_available called without new data available";
        }
        uint8_t dummy_buffer[buffer_size];
        this->transport->receive_data(dummy_buffer, sizeof(dummy_buffer));
        done->raise();
    }));
    this->transport->register_observer(observer);

    EXPECT_FALSE(done->raised());
    uint8_t dummy_buffer[buffer_size];
    memset(dummy_buffer, 0xab, sizeof(dummy_buffer));
    EXPECT_EQ(sizeof(dummy_buffer), write(this->test_fd, dummy_buffer, sizeof(dummy_buffer)));

    EXPECT_TRUE(done->wait_for(std::chrono::seconds{1}));

    std::this_thread::sleep_for(std::chrono::seconds{1});
}

TYPED_TEST(StreamTransportTest, DoesntSendDataAvailableNotificationOnDisconnect)
{
    using namespace testing;
    int const buffer_size{256};

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto read_done = std::make_shared<mir::test::Signal>();
    auto disconnect_done = std::make_shared<mir::test::Signal>();
    std::atomic<int> notify_count{0};

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([this, read_done, &notify_count]()
    {
        notify_count++;
        uint8_t dummy_buffer[buffer_size];
        this->transport->receive_data(dummy_buffer, sizeof(dummy_buffer));
        read_done->raise();
    }));
    ON_CALL(*observer, on_disconnected())
        .WillByDefault(Invoke([this, disconnect_done]() { disconnect_done->raise(); }));

    this->transport->register_observer(observer);

    EXPECT_FALSE(read_done->raised());
    uint8_t dummy_buffer[buffer_size];
    memset(dummy_buffer, 0xab, sizeof(dummy_buffer));
    EXPECT_EQ(sizeof(dummy_buffer), write(this->test_fd, dummy_buffer, sizeof(dummy_buffer)));

    EXPECT_TRUE(read_done->wait_for(std::chrono::seconds{1}));
    EXPECT_EQ(1, notify_count);

    ::close(this->test_fd);
    EXPECT_TRUE(disconnect_done->wait_for(std::chrono::seconds{1}));

    EXPECT_EQ(1, notify_count);
}

TYPED_TEST(StreamTransportTest, ReadsCorrectData)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    std::string expected{"I am the very model of a modern major general"};
    std::vector<char> received(expected.size());

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([done, &received, this]()
    {
        this->transport->receive_data(received.data(), received.size());
        done->raise();
    }));

    this->transport->register_observer(observer);

    EXPECT_EQ(expected.size(), write(this->test_fd, expected.data(), expected.size()));

    ASSERT_TRUE(done->wait_for(std::chrono::seconds{1}));
    EXPECT_EQ(0, memcmp(expected.data(), received.data(), expected.size()));
}

TYPED_TEST(StreamTransportTest, WritesCorrectData)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    auto done = std::make_shared<mir::test::Signal>();

    std::string expected{"I am the very model of a modern major general"};
    std::vector<uint8_t> send_buffer{expected.begin(), expected.end()};
    std::vector<char> received(expected.size());

    this->transport->send_data(send_buffer);

    pollfd read_listener;
    read_listener.fd = this->test_fd;
    read_listener.events = POLLIN;

    ASSERT_EQ(1, poll(&read_listener, 1, 1000)) << "Failed to poll(): " << strerror(errno);

    EXPECT_EQ(expected.size(), read(this->test_fd, received.data(), received.size()));
    EXPECT_EQ(0, memcmp(expected.data(), received.data(), expected.size()));
}

namespace
{
bool alarm_raised;
void set_alarm_raised(int /*signo*/)
{
    alarm_raised = true;
}
}

TYPED_TEST(StreamTransportTest, ReadsFullDataFromMultipleChunks)
{
    size_t const chunk_size{8};
    std::vector<uint8_t> expected(chunk_size * 4);

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size());

    mir::test::AutoJoinThread reader([&]()
    {
        this->transport->receive_data(received.data(), received.size());
    });

    size_t bytes_written{0};
    while (bytes_written < expected.size())
    {
        auto result = send(this->test_fd,
                           expected.data() + bytes_written,
                           std::min(chunk_size, expected.size() - bytes_written),
                           MSG_DONTWAIT);

        ASSERT_NE(-1, result) << "Failed to send(): " << strerror(errno);
        bytes_written += result;
    }

    reader.stop();

    EXPECT_EQ(expected, received);
}

TYPED_TEST(StreamTransportTest, ReadsFullDataWhenInterruptedWithSignals)
{
    size_t const chunk_size{1024};
    std::vector<uint8_t> expected(chunk_size * 4);

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size());

    struct sigaction old_handler;

    auto sig_alarm_handler = mir::raii::paired_calls([&old_handler]()
    {
        struct sigaction alarm_handler;
        sigset_t blocked_signals;

        sigemptyset(&blocked_signals);
        alarm_handler.sa_handler = &set_alarm_raised;
        alarm_handler.sa_flags = 0;
        alarm_handler.sa_mask = blocked_signals;
        if (sigaction(SIGALRM, &alarm_handler, &old_handler) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to set SIGALRM handler"};
        }
    },
    [&old_handler]()
    {
        if (sigaction(SIGALRM, &old_handler, nullptr) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to restore SIGALRM handler"};
        }
    });

    auto read_now_waiting = std::make_shared<mir::test::Signal>();

    mir::test::AutoJoinThread reader([&]()
    {
        alarm_raised = false;
        read_now_waiting->raise();
        this->transport->receive_data(received.data(), received.size());
        EXPECT_TRUE(alarm_raised);
    });

    EXPECT_TRUE(read_now_waiting->wait_for(std::chrono::seconds{1}));

    pthread_kill(reader.native_handle(), SIGALRM);

    size_t bytes_written{0};
    while (bytes_written < expected.size())
    {
        pollfd socket_writable;
        socket_writable.events = POLLOUT;
        socket_writable.fd = this->test_fd;

        ASSERT_GE(poll(&socket_writable, 1, 10000), 1);
        ASSERT_EQ(0, socket_writable.revents & (POLLERR | POLLHUP));

        auto result = send(this->test_fd,
                           expected.data() + bytes_written,
                           std::min(chunk_size, expected.size() - bytes_written),
                           MSG_DONTWAIT);

        ASSERT_NE(-1, result) << "Failed to send(): " << strerror(errno);
        bytes_written += result;
        pthread_kill(reader.native_handle(), SIGALRM);
    }

    reader.stop();

    EXPECT_EQ(expected, received);
}

namespace
{
template<std::size_t N>
ssize_t send_with_fds(int socket, std::array<int, N> fds, void* msg_data, size_t msg_size, int flags)
{
    struct iovec iov;
    iov.iov_base = msg_data;
    iov.iov_len = msg_size;

    // Allocate space for control message
    static auto const fds_bytes = fds.size() * sizeof(int);
    std::array<int, CMSG_SPACE(fds_bytes)> control;
    memset(control.data(), 0, control.size());

    // Message to send
    struct msghdr header;
    header.msg_name = NULL;
    header.msg_namelen = 0;
    header.msg_iov = &iov;
    header.msg_iovlen = 1;
    header.msg_controllen = control.size();
    header.msg_control = control.data();
    header.msg_flags = 0;

    // Control message contains file descriptors
    struct cmsghdr *message = CMSG_FIRSTHDR(&header);
    message->cmsg_len = CMSG_LEN(fds_bytes);
    message->cmsg_level = SOL_SOCKET;
    message->cmsg_type = SCM_RIGHTS;

    int* const data = reinterpret_cast<int*>(CMSG_DATA(message));
    memcpy(data, fds.data(), fds.size() * sizeof(int));

    auto const sent = sendmsg(socket, &header, flags);
    if (sent < 0)
        throw std::system_error(errno, std::system_category(), "Failed to send data + fds");
    return sent;
}

::testing::AssertionResult fds_are_equivalent(char const* fd_one_expr,
                                              char const* fd_two_expr,
                                              int fd_one,
                                              int fd_two)
{
    std::string first_proc_path{"/proc/self/fd/" +
                                std::to_string(fd_one)};
    std::string second_proc_path{"/proc/self/fd/" +
                                 std::to_string(fd_two)};

    struct stat sb;

    if(lstat(first_proc_path.c_str(), &sb) < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to stat fd" + std::to_string(fd_one));
    }

    std::vector<char> first_path(sb.st_size + 1, '\0');
    auto result = readlink(first_proc_path.c_str(), first_path.data(), sb.st_size);
    if (result < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to find fd's path");
    }

    if(lstat(second_proc_path.c_str(), &sb) < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to stat fd" + std::to_string(fd_two));
    }

    std::vector<char> second_path(sb.st_size + 1, '\0');
    result = readlink(second_proc_path.c_str(), second_path.data(), sb.st_size);
    if (result < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to find fd's path");
    }

    // For more reliability we could compare stat results, check inode numbers, etc
    // This should do, though.

    if (first_path == second_path)
        return testing::AssertionSuccess() << fd_one_expr << " and " << fd_two_expr
                                           << " both point to " << first_path.data();

    return testing::AssertionFailure() << fd_one_expr << " and " << fd_two_expr
                                       << " are not equivalent: "
                                       << fd_one_expr << " corresponds to " << first_path.data() << std::endl
                                       << fd_two_expr << " corresponds to " << second_path.data();
}


class TestFd
{
public:
    TestFd()
        : filename(strlen(file_template) + 1, '\0')
    {
        strncpy(filename.data(), file_template, filename.size());
        fd = mkstemp(filename.data());
        if (fd < 0)
            throw std::system_error(errno, std::system_category(), "Failed to create test fd");
    }

    ~TestFd()
    {
        unlink(filename.data());
        close(fd);
    }

    int fd;
    static constexpr char const* file_template = "/tmp/mir-test-file-XXXXXX";
    std::vector<char> filename;
};
}

TYPED_TEST(StreamTransportTest, ReadsDataWithFds)
{
    size_t const chunk_size{8};
    int const num_chunks{4};
    int const num_fds{6};
    std::vector<uint8_t> expected(chunk_size * num_chunks);

    std::array<TestFd, num_fds> test_files;
    std::array<int, num_fds> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size());
    std::vector<int> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread receive_thread{[this]() { ::close(this->test_fd); },
                                                [&]()
    {
        try
        {
            this->transport->receive_data(received.data(), received.size(), received_fds);
        }
        catch (std::exception& e)
        {
            FAIL() << "Exception caught while reading data: " << e.what();
        }
        receive_done->raise();
    }};

    EXPECT_EQ(expected.size(), send_with_fds(this->test_fd,
                                             test_fds,
                                             expected.data(),
                                             expected.size(),
                                             MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));
    EXPECT_EQ(expected, received);

    // Man, I'd really love std::zip() here...
    for (unsigned int i = 0; i < test_files.size(); ++i)
    {
        EXPECT_PRED_FORMAT2(fds_are_equivalent, test_files[i].fd, received_fds[i]);
        ::close(received_fds[i]);
    }
}

TYPED_TEST(StreamTransportTest, ReadsFullDataAndFdsWhenInterruptedWithSignals)
{
    size_t const chunk_size{1024};
    int const num_chunks{4};
    int const num_fds{5};
    std::vector<uint8_t> expected(chunk_size * num_chunks);

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::array<TestFd, num_fds> test_files;
    std::array<int, num_fds> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    std::vector<uint8_t> received(expected.size());
    std::vector<int> received_fds(num_fds);

    struct sigaction old_handler;

    auto sig_alarm_handler = mir::raii::paired_calls([&old_handler]()
    {
        struct sigaction alarm_handler;
        sigset_t blocked_signals;

        sigemptyset(&blocked_signals);
        alarm_handler.sa_handler = &set_alarm_raised;
        alarm_handler.sa_flags = 0;
        alarm_handler.sa_mask = blocked_signals;
        if (sigaction(SIGALRM, &alarm_handler, &old_handler) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to set SIGALRM handler"};
        }
    },
    [&old_handler]()
    {
        if (sigaction(SIGALRM, &old_handler, nullptr) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to restore SIGALRM handler"};
        }
    });

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread reader{[this]() { ::close(this->test_fd); },
                                        [&]()
    {
        try
        {
            alarm_raised = false;
            this->transport->receive_data(received.data(), received.size(), received_fds);
            EXPECT_TRUE(alarm_raised);
        }
        catch (std::exception& e)
        {
            FAIL() << "Exception caught while reading data: " << e.what();
        }
        receive_done->raise();
    }};

    pthread_kill(reader.native_handle(), SIGALRM);

    size_t bytes_written{0};
    while (bytes_written < expected.size())
    {
        pollfd socket_writable;
        socket_writable.events = POLLOUT;
        socket_writable.fd = this->test_fd;

        ASSERT_GE(poll(&socket_writable, 1, 10000), 1);
        ASSERT_EQ(0, socket_writable.revents & (POLLERR | POLLHUP));

        ssize_t result;
        if (bytes_written + chunk_size < expected.size())
        {
            result = send(this->test_fd,
                          expected.data() + bytes_written,
                          std::min(chunk_size, expected.size() - bytes_written),
                          MSG_DONTWAIT);
            ASSERT_NE(-1, result) << "Failed to send(): " << strerror(errno);
        }
        else
        {
            result = send_with_fds(this->test_fd,
                                   test_fds,
                                   expected.data() + bytes_written,
                                   std::min(chunk_size, expected.size() - bytes_written),
                                   MSG_DONTWAIT);
        }
        bytes_written += result;
        pthread_kill(reader.native_handle(), SIGALRM);
    }

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));

    EXPECT_EQ(expected, received);

    // Man, I'd really love std::zip() here...
    for (unsigned int i = 0; i < test_files.size(); ++i)
    {
        EXPECT_PRED_FORMAT2(fds_are_equivalent, test_files[i].fd, received_fds[i]);
        ::close(received_fds[i]);
    }
}

namespace
{
/*
 * Find the first integer n ≥ starting_fd_count where the CMSG_LEN for sending
 * n fds is different to the CMSG_LEN for sending n+1 fds.
 *
 * Note: because there's an alignment constraint on CMSG_LEN, this is not necessarily
 * just starting_fd_count.
 */
constexpr int cmsg_len_boundary(int starting_fd_count)
{
    return CMSG_LEN(starting_fd_count * sizeof(int)) == CMSG_LEN((starting_fd_count + 1) * sizeof(int)) ?
           cmsg_len_boundary(starting_fd_count + 1) :
           starting_fd_count;
}
}

TYPED_TEST(StreamTransportTest, ThrowsErrorWhenReceivingMoreFdsThanRequested)
{
    constexpr int num_fds{cmsg_len_boundary(1)};

    std::array<TestFd, num_fds + 1> test_files;
    std::array<int, num_fds + 1> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    std::vector<int> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread reader{[this]() { ::close(this->test_fd); },
                                        [&]()
    {
        uint32_t dummy;
        EXPECT_THROW(this->transport->receive_data(&dummy, sizeof(dummy), received_fds),
                     std::runtime_error);
        receive_done->raise();
    }};

    int32_t dummy{0};
    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd,
                                           test_fds,
                                           &dummy,
                                           sizeof(dummy),
                                           MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));

}

TYPED_TEST(StreamTransportTest, ReturnsValidWatchFd)
{
    // A valid fd is >= 0, and we know that stdin, stdout, and stderr aren't correct.
    EXPECT_GE(this->transport->watch_fd(), 3);
}
