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
#include "mir/fd.h"

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

::testing::AssertionResult std_call_succeeded(int retval)
{
    if (retval >= 0)
    {
        return ::testing::AssertionSuccess();
    }
    else
    {
        return ::testing::AssertionFailure() << "errno: "
                                             << errno
                                             << " ["
                                             << strerror(errno)
                                             << "]";
    }
}

template<typename Period, typename Rep>
::testing::AssertionResult fd_becomes_readable(mir::Fd const& fd,
                                               std::chrono::duration<Period, Rep> timeout)
{
    int timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();

    pollfd readable;
    readable.events = POLLIN;
    readable.fd = fd;

    auto result = std_call_succeeded(poll(&readable, 1, timeout_ms));
    if (result)
    {
        if (readable.revents & POLLERR)
        {
            return ::testing::AssertionFailure() << "error condition on fd";
        }
        if (readable.revents & POLLNVAL)
        {
            return ::testing::AssertionFailure() << "fd is invalid";
        }
        if (!(readable.revents & POLLIN))
        {
            return ::testing::AssertionFailure() << "fd is not readable";
        }
        return ::testing::AssertionSuccess();
    }
    return result;
}

::testing::AssertionResult fd_is_readable(mir::Fd const& fd)
{
    return fd_becomes_readable(fd, std::chrono::seconds{0});
}
}

template <typename TransportMechanism>
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

        test_fd = mir::Fd{socket_fds[0]};
        transport_fd = mir::Fd{socket_fds[1]};
        transport = std::make_shared<TransportMechanism>(transport_fd);
    }

    mir::Fd transport_fd;
    mir::Fd test_fd;
    std::shared_ptr<TransportMechanism> transport;
};

typedef ::testing::Types<mclr::StreamSocketTransport> Transports;
TYPED_TEST_CASE(StreamTransportTest, Transports);

TYPED_TEST(StreamTransportTest, ReturnsValidWatchFd)
{
    // A valid fd is >= 0, and we know that stdin, stdout, and stderr aren't correct.
    EXPECT_GE(this->transport->watch_fd(), 3);
}

TYPED_TEST(StreamTransportTest, WatchFdIsPollable)
{
    pollfd socket_readable;
    socket_readable.events = POLLIN;
    socket_readable.fd = this->transport->watch_fd();

    ASSERT_TRUE(std_call_succeeded(poll(&socket_readable, 1, 0)));

    EXPECT_FALSE(socket_readable.revents & POLLERR);
    EXPECT_FALSE(socket_readable.revents & POLLNVAL);
}

TYPED_TEST(StreamTransportTest, WatchFdNotifiesReadableWhenDataPending)
{
    pollfd socket_readable;
    socket_readable.events = POLLIN;
    socket_readable.fd = this->transport->watch_fd();

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    ASSERT_TRUE(std_call_succeeded(poll(&socket_readable, 1, 1000)));

    ASSERT_FALSE(socket_readable.revents & POLLERR);
    ASSERT_FALSE(socket_readable.revents & POLLNVAL);

    EXPECT_TRUE(socket_readable.revents & POLLIN);
}

TYPED_TEST(StreamTransportTest, WatchFdRemainsUnreadableUntilEventPending)
{
    EXPECT_FALSE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, WatchFdIsNoLongerReadableAfterEventProcessing)
{
    using namespace testing;

    pollfd socket_readable;
    socket_readable.events = POLLIN;
    socket_readable.fd = this->transport->watch_fd();

    uint64_t dummy{0xdeadbeef};

    auto observer = std::make_shared<NiceMock<MockObserver>>();

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([dummy, this]()
                              {
                                  decltype(dummy) buffer;
                                  this->transport->receive_data(&buffer, sizeof(dummy));
                              }));

    this->transport->register_observer(observer);

    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    ASSERT_TRUE(std_call_succeeded(poll(&socket_readable, 1, 1000)));

    ASSERT_FALSE(socket_readable.revents & POLLERR);
    ASSERT_FALSE(socket_readable.revents & POLLNVAL);

    EXPECT_TRUE(socket_readable.revents & POLLIN);

    this->transport->dispatch();

    ASSERT_TRUE(std_call_succeeded(poll(&socket_readable, 1, 0)));

    EXPECT_FALSE(socket_readable.revents & POLLIN);
}

TYPED_TEST(StreamTransportTest, NoEventsDispatchedUntilDispatchCalled)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    bool data_available{false};
    bool disconnected{false};

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));
    ::close(this->test_fd);

    ON_CALL(*observer, on_data_available()).WillByDefault(Invoke([this, dummy, &data_available]()
                                                                 {
                                                                     decltype(dummy) buffer;
                                                                     this->transport->receive_data(&buffer, sizeof(buffer));
                                                                     data_available = true;
                                                                 }));
    ON_CALL(*observer, on_disconnected()).WillByDefault(Invoke([&disconnected]()
                                                               { disconnected = true; }));

    this->transport->register_observer(observer);

    std::this_thread::sleep_for(std::chrono::seconds{1});
    EXPECT_FALSE(data_available);
    EXPECT_FALSE(disconnected);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

    EXPECT_TRUE(data_available);
    EXPECT_TRUE(disconnected);
}

TYPED_TEST(StreamTransportTest, DispatchesSingleEventAtATime)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    bool data_available{false};
    bool disconnected{false};

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));
    ::close(this->test_fd);

    ON_CALL(*observer, on_data_available()).WillByDefault(Invoke([this, dummy, &data_available]()
                                                                 {
                                                                     decltype(dummy) buffer;
                                                                     this->transport->receive_data(&buffer, sizeof(buffer));
                                                                     data_available = true;
                                                                 }));
    ON_CALL(*observer, on_disconnected()).WillByDefault(Invoke([&disconnected]()
                                                               { disconnected = true; }));

    this->transport->register_observer(observer);

    EXPECT_FALSE(data_available);
    EXPECT_FALSE(disconnected);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));

    this->transport->dispatch();

    EXPECT_TRUE(data_available xor disconnected);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));

    this->transport->dispatch();

    EXPECT_TRUE(data_available);
    EXPECT_TRUE(disconnected);
}

TYPED_TEST(StreamTransportTest, NoticesRemoteDisconnect)
{
    using namespace testing;
    auto observer = std::make_shared<NiceMock<MockObserver>>();
    bool disconnected{false};

    ON_CALL(*observer, on_disconnected()).WillByDefault(Invoke([&disconnected]()
                                                               { disconnected = true; }));

    this->transport->register_observer(observer);

    close(this->test_fd);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

    EXPECT_TRUE(disconnected);
}

TYPED_TEST(StreamTransportTest, NoticesRemoteDisconnectWhileReading)
{
    using namespace testing;
    auto observer = std::make_shared<NiceMock<MockObserver>>();
    bool disconnected{false};
    bool receive_error_detected{false};

    ON_CALL(*observer, on_disconnected()).WillByDefault(Invoke([&disconnected]()
                                                               { disconnected = true; }));
    this->transport->register_observer(observer);

    mir::test::AutoJoinThread closer{[this]()
                                     {
                                         std::this_thread::sleep_for(std::chrono::seconds{1});
                                         ::close(this->test_fd);
                                     }};

    try
    {
        char buffer[8];
        this->transport->receive_data(buffer, sizeof(buffer));
    }
    catch (std::runtime_error)
    {
        receive_error_detected = true;
    }

    // There should now be a disconnect event pending...
    EXPECT_TRUE(fd_is_readable(this->transport->watch_fd()));

    this->transport->dispatch();

    EXPECT_TRUE(disconnected);
    EXPECT_TRUE(receive_error_detected);
}

TYPED_TEST(StreamTransportTest, NotifiesOnDataAvailable)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    bool notified_data_available{false};

    ON_CALL(*observer, on_data_available()).WillByDefault(Invoke([&notified_data_available]()
                                                                 { notified_data_available = true; }));

    this->transport->register_observer(observer);

    uint64_t dummy{0xdeadbeef};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));

    this->transport->dispatch();

    EXPECT_TRUE(notified_data_available);
}

TYPED_TEST(StreamTransportTest, KeepsNotifyingOfAvailableDataUntilAllIsRead)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();

    std::array<uint8_t, sizeof(int) * 256> data;
    data.fill(0);
    size_t bytes_left{data.size()};

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([&bytes_left, this]()
                              {
                                  int dummy;
                                  this->transport->receive_data(&dummy, sizeof(dummy));
                                  bytes_left -= sizeof(dummy);
                              }));

    this->transport->register_observer(observer);

    EXPECT_EQ(data.size(), write(this->test_fd, data.data(), data.size()));

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

    EXPECT_EQ(0, bytes_left);
}

TYPED_TEST(StreamTransportTest, StopsNotifyingOnceAllDataIsRead)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();

    std::array<uint8_t, sizeof(int) * 256> data;
    data.fill(0);
    size_t bytes_left{data.size()};

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([&bytes_left, this]()
                              {
                                  int dummy;
                                  this->transport->receive_data(&dummy, sizeof(dummy));
                                  bytes_left -= sizeof(dummy);
                              }));

    this->transport->register_observer(observer);

    EXPECT_EQ(data.size(), write(this->test_fd, data.data(), data.size()));

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (bytes_left > 0)
    {
        this->transport->dispatch();
    }

    EXPECT_FALSE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, DoesntSendDataAvailableNotificationOnDisconnect)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();
    int notify_count{0};
    bool disconnected{false};

    uint64_t dummy{0xdeedfaac};
    EXPECT_EQ(sizeof(dummy), write(this->test_fd, &dummy, sizeof(dummy)));

    ON_CALL(*observer, on_disconnected()).WillByDefault(Invoke([&disconnected]()
                                                               { disconnected = true; }));
    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([dummy, &notify_count, this]()
                              {
                                  notify_count++;

                                  decltype(dummy) buffer;
                                  this->transport->receive_data(&buffer, sizeof(buffer));
                              }));

    this->transport->register_observer(observer);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

    ::close(this->test_fd);

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

    EXPECT_FALSE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    EXPECT_EQ(1, notify_count);
    EXPECT_TRUE(disconnected);
}

TYPED_TEST(StreamTransportTest, ReadsCorrectData)
{
    using namespace testing;

    auto observer = std::make_shared<NiceMock<MockObserver>>();

    std::string expected{"I am the very model of a modern major general"};
    std::vector<char> received(expected.size());

    ON_CALL(*observer, on_data_available())
        .WillByDefault(Invoke([&received, this]()
                              {
                                  this->transport->receive_data(received.data(), received.size());
                              }));

    this->transport->register_observer(observer);

    EXPECT_EQ(expected.size(), write(this->test_fd, expected.data(), expected.size()));

    EXPECT_TRUE(fd_becomes_readable(this->transport->watch_fd(), std::chrono::seconds{1}));
    while (fd_is_readable(this->transport->watch_fd()))
    {
        this->transport->dispatch();
    }

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

class SocketBlockThreshold
{
public:
    SocketBlockThreshold(int send_fd, int recv_fd) : send_fd{send_fd}, recv_fd{recv_fd}
    {
        int val{256};
        socklen_t val_size{sizeof(val)};

        getsockopt(recv_fd, SOL_SOCKET, SO_RCVBUF, &old_recvbuf, &val_size);
        getsockopt(send_fd, SOL_SOCKET, SO_SNDBUF, &old_sndbuf, &val_size);

        setsockopt(recv_fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
        setsockopt(send_fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));
    }

    ~SocketBlockThreshold()
    {
        setsockopt(recv_fd, SOL_SOCKET, SO_SNDBUF, &old_sndbuf, sizeof(old_sndbuf));
        setsockopt(send_fd, SOL_SOCKET, SO_RCVBUF, &old_recvbuf, sizeof(old_recvbuf));
    }

    size_t data_size_which_should_block()
    {
        int sendbuf, recvbuf;
        socklen_t buf_size{sizeof(sendbuf)};
        getsockopt(recv_fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, &buf_size);
        getsockopt(send_fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &buf_size);

        return sendbuf + recvbuf;
    }

private:
    int send_fd, recv_fd;
    int old_sndbuf, old_recvbuf;
};

class TemporarySignalHandler
{
public:
    TemporarySignalHandler(int signo, void (*handler)(int)) : signo{signo}
    {
        struct sigaction alarm_handler;
        sigset_t blocked_signals;

        sigemptyset(&blocked_signals);
        alarm_handler.sa_handler = handler;
        alarm_handler.sa_flags = 0;
        alarm_handler.sa_mask = blocked_signals;
        if (sigaction(signo, &alarm_handler, &old_handler) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to set signal handler"};
        }
    }
    ~TemporarySignalHandler()
    {
        if (sigaction(signo, &old_handler, nullptr) < 0)
        {
            throw std::system_error{errno, std::system_category(), "Failed to restore SIGALRM handler"};
        }
    }

private:
    int const signo;
    struct sigaction old_handler;
};
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
                                     { this->transport->receive_data(received.data(), received.size()); });

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
    SocketBlockThreshold sockopt{this->test_fd, this->transport_fd};

    size_t const chunk_size{sockopt.data_size_which_should_block()};
    std::vector<uint8_t> expected(chunk_size * 4);

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size());

    TemporarySignalHandler sig_alarm_handler{SIGALRM, &set_alarm_raised};
    auto read_now_waiting = std::make_shared<mir::test::Signal>();

    mir::test::AutoJoinThread reader([&]()
                                     {
                                         alarm_raised = false;
                                         read_now_waiting->raise();
                                         this->transport->receive_data(received.data(), received.size());
                                         EXPECT_TRUE(alarm_raised);
                                     });

    EXPECT_TRUE(read_now_waiting->wait_for(std::chrono::seconds{1}));

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
template <std::size_t N>
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
    struct cmsghdr* message = CMSG_FIRSTHDR(&header);
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

::testing::AssertionResult fds_are_equivalent(char const* fd_one_expr, char const* fd_two_expr, int fd_one, int fd_two)
{
    std::string first_proc_path{"/proc/self/fd/" + std::to_string(fd_one)};
    std::string second_proc_path{"/proc/self/fd/" + std::to_string(fd_two)};

    struct stat sb;

    if (lstat(first_proc_path.c_str(), &sb) < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to stat fd" + std::to_string(fd_one));
    }

    std::vector<char> first_path(sb.st_size + 1, '\0');
    auto result = readlink(first_proc_path.c_str(), first_path.data(), sb.st_size);
    if (result < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to find fd's path");
    }

    if (lstat(second_proc_path.c_str(), &sb) < 0)
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
        return testing::AssertionSuccess() << fd_one_expr << " and " << fd_two_expr << " both point to "
                                           << first_path.data();

    return testing::AssertionFailure() << fd_one_expr << " and " << fd_two_expr
                                       << " are not equivalent: " << fd_one_expr << " corresponds to "
                                       << first_path.data() << std::endl << fd_two_expr << " corresponds to "
                                       << second_path.data();
}

class TestFd
{
public:
    TestFd() : filename(strlen(file_template) + 1, '\0')
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
    std::vector<mir::Fd> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread receive_thread{[this]()
                                                { ::close(this->test_fd); },
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

    EXPECT_EQ(expected.size(), send_with_fds(this->test_fd, test_fds, expected.data(), expected.size(), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));
    EXPECT_EQ(expected, received);

    // Man, I'd really love std::zip() here...
    for (unsigned int i = 0; i < test_files.size(); ++i)
    {
        EXPECT_PRED_FORMAT2(fds_are_equivalent, test_files[i].fd, received_fds[i]);
        ::close(received_fds[i]);
    }
}

TYPED_TEST(StreamTransportTest, ReadsFdsFromMultipleChunks)
{
    size_t const chunk_size{8};
    int const num_chunks{4};
    int const num_fds{6};
    std::vector<uint8_t> expected(chunk_size * num_chunks);

    std::array<TestFd, num_fds> first_test_files;
    std::array<TestFd, num_fds> second_test_files;
    std::array<int, num_fds> first_test_fds;
    std::array<int, num_fds> second_test_fds;
    for (unsigned int i = 0; i < num_fds; ++i)
    {
        first_test_fds[i] = first_test_files[i].fd;
        second_test_fds[i] = second_test_files[i].fd;
    }

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size() * 2);
    std::vector<mir::Fd> received_fds(num_fds * 2);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread receive_thread{[this]()
                                                { ::close(this->test_fd); },
                                                [&]()
                                                {
        try
        {
            this->transport->receive_data(received.data(), received.size(), received_fds);
        }
        catch (std::exception& e)
        {
            ADD_FAILURE() << "Exception caught while reading data: " << e.what();
        }
        receive_done->raise();
    }};

    EXPECT_EQ(expected.size(),
              send_with_fds(this->test_fd, first_test_fds, expected.data(), expected.size(), MSG_DONTWAIT));
    EXPECT_EQ(expected.size(),
              send_with_fds(this->test_fd, second_test_fds, expected.data(), expected.size(), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));

    // Man, I'd really love std::zip() here...
    for (unsigned int i = 0; i < num_fds; ++i)
    {
        EXPECT_PRED_FORMAT2(fds_are_equivalent, first_test_files[i].fd, received_fds[i]);
        EXPECT_PRED_FORMAT2(fds_are_equivalent, second_test_files[i].fd, received_fds[i + num_fds]);
        ::close(received_fds[i]);
        ::close(received_fds[i + num_fds]);
    }
}

TYPED_TEST(StreamTransportTest, ReadsFullDataAndFdsWhenInterruptedWithSignals)
{
    SocketBlockThreshold sockopt{this->test_fd, this->transport_fd};

    size_t const chunk_size{sockopt.data_size_which_should_block()};
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
    std::vector<mir::Fd> received_fds(num_fds);

    TemporarySignalHandler sig_alarm_handler{SIGALRM, &set_alarm_raised};
    auto receive_done = std::make_shared<mir::test::Signal>();

    mir::test::AutoUnblockThread reader{[this]()
                                        { ::close(this->test_fd); },
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
            ADD_FAILURE() << "Exception caught while reading data: " << e.what();
        }
        receive_done->raise();
    }};

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
        std::this_thread::yield();
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
 * Find the first integer n ≥ starting_fd_count where the CMSG_SPACE for sending
 * n fds is different to the CMSG_SPACE for sending n+1 fds.
 *
 * Note: because there are alignment constraints, this is not necessarily
 * just starting_fd_count.
 */
constexpr int cmsg_space_boundary(int starting_fd_count)
{
    return CMSG_SPACE(starting_fd_count * sizeof(int)) != CMSG_SPACE((starting_fd_count + 1) * sizeof(int)) ?
               starting_fd_count :
               cmsg_space_boundary(starting_fd_count + 1);
}

/*
 * Find the first integer n ≥ starting_fd_count where the CMSG_SPACE for sending
 * n fds is the same as the CMSG_SPACE for sending n+1 fds.
 *
 * Note: because there are alignment constraints, this can exist
 *
 * Returns max_count if no such n has been found such that n < max_count;
 * this occurs on some architectures.
 */
constexpr int cmsg_space_alias(int starting_fd_count, int max_count)
{
    return CMSG_SPACE(starting_fd_count * sizeof(int)) == CMSG_SPACE((starting_fd_count + 1) * sizeof(int)) ?
               starting_fd_count :
               starting_fd_count >= max_count ? max_count : cmsg_space_alias(starting_fd_count + 1, max_count);
}
}

TYPED_TEST(StreamTransportTest, ReceivingMoreFdsThanExpectedOnCmsgBoundaryIsAnError)
{
    constexpr int num_fds{cmsg_space_boundary(1)};

    std::array<TestFd, num_fds + 1> test_files;
    std::array<int, num_fds + 1> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    std::vector<mir::Fd> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread reader{[this]()
                                        { ::close(this->test_fd); },
                                        [&]()
                                        {
        uint32_t dummy;
        EXPECT_THROW(this->transport->receive_data(&dummy, sizeof(dummy), received_fds), std::runtime_error);
        receive_done->raise();
    }};

    int32_t dummy{0};
    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd, test_fds, &dummy, sizeof(dummy), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));
}

TYPED_TEST(StreamTransportTest, ReceivingMoreFdsThanRequestedWithSameCmsgSpaceIsAnError)
{
    constexpr int num_fds{cmsg_space_alias(1, 20)};

    std::array<TestFd, num_fds + 1> test_files;
    std::array<int, num_fds + 1> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    std::vector<mir::Fd> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread reader{[this]()
                                        { ::close(this->test_fd); },
                                        [&]()
                                        {
        uint32_t dummy;
        EXPECT_THROW(this->transport->receive_data(&dummy, sizeof(dummy), received_fds), std::runtime_error);
        receive_done->raise();
    }};

    int32_t dummy{0};
    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd, test_fds, &dummy, sizeof(dummy), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));
}

/* Amusingly, if recvmsg gets interrupted while reading a control message
 * it turns out that we get back the fds, and then the next read will
 * *also* get back (duplicates of) the fds.
 *
 * This would seem to be maybe a kernel bug, but this makes it impossible to
 * distinguish between “we were expecting some fds, but the server sent us
 * twice as many in two batches” and “recvmsg was interrupted and we got the same
 * fds twice”
 *
 * Rad.
 */
TYPED_TEST(StreamTransportTest, DISABLED_ReceivingMoreFdsThanExpectedInMultipleChunksRaisesException)
{
    size_t const chunk_size{8};
    int const num_chunks{4};
    int const num_fds{6};
    std::vector<uint8_t> expected(chunk_size * num_chunks);

    std::array<TestFd, num_fds> first_test_files;
    std::array<TestFd, num_fds> second_test_files;
    std::array<int, num_fds> first_test_fds;
    std::array<int, num_fds> second_test_fds;
    for (unsigned int i = 0; i < num_fds; ++i)
    {
        first_test_fds[i] = first_test_files[i].fd;
        second_test_fds[i] = second_test_files[i].fd;
    }

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size() * 2);
    std::vector<mir::Fd> received_fds(num_fds);

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread receive_thread{[this]()
                                                { ::close(this->test_fd); },
                                                [&]()
                                                {
        EXPECT_THROW(this->transport->receive_data(received.data(), received.size(), received_fds), std::runtime_error);
        receive_done->raise();
    }};

    EXPECT_EQ(expected.size(),
              send_with_fds(this->test_fd, first_test_fds, expected.data(), expected.size(), MSG_DONTWAIT));
    EXPECT_EQ(expected.size(),
              send_with_fds(this->test_fd, second_test_fds, expected.data(), expected.size(), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));

    // Man, I'd really love std::zip() here...
    for (unsigned int i = 0; i < num_fds; ++i)
    {
        EXPECT_PRED_FORMAT2(fds_are_equivalent, first_test_files[i].fd, received_fds[i]);
        ::close(received_fds[i]);
    }
}

TYPED_TEST(StreamTransportTest, MismatchedFdExpectationsHaveAppropriateErrorMessages)
{
    constexpr int num_fds{5};

    std::array<TestFd, num_fds> test_files;
    std::array<int, num_fds> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    int32_t dummy{0};
    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd, test_fds, &dummy, sizeof(dummy), MSG_DONTWAIT));

    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd, test_fds, &dummy, sizeof(dummy), MSG_DONTWAIT));

    try
    {
        std::vector<mir::Fd> dummy_fds(num_fds + 1);
        this->transport->receive_data(&dummy, sizeof(dummy), dummy_fds);
        FAIL() << "Receiving fewer fds than sent unexpectedly succeeded";
    }
    catch (std::runtime_error const& err)
    {
        EXPECT_THAT(err.what(), testing::HasSubstr("fewer fds than expected"));
    }

    try
    {
        std::vector<mir::Fd> dummy_fds(num_fds - 1);
        this->transport->receive_data(&dummy, sizeof(dummy), dummy_fds);
        FAIL() << "Receiving more fds than sent unexpectedly succeeded";
    }
    catch (std::runtime_error const& err)
    {
        EXPECT_THAT(err.what(), testing::HasSubstr("more fds than expected"));
    }
}

TYPED_TEST(StreamTransportTest, SendsFullMessagesWhenInterrupted)
{
    SocketBlockThreshold socketopt(this->transport_fd, this->test_fd);

    size_t const chunk_size{socketopt.data_size_which_should_block()};
    std::vector<uint8_t> expected(chunk_size * 4);

    uint8_t counter{0};
    for (auto& byte : expected)
    {
        byte = counter++;
    }
    std::vector<uint8_t> received(expected.size());

    TemporarySignalHandler sig_alarm_handler{SIGALRM, &set_alarm_raised};
    auto write_now_waiting = std::make_shared<mir::test::Signal>();

    mir::test::AutoJoinThread writer([&]()
                                     {
                                         alarm_raised = false;
                                         write_now_waiting->raise();
                                         this->transport->send_data(expected);
                                         EXPECT_TRUE(alarm_raised);
                                     });

    size_t bytes_read{0};
    while (bytes_read < received.size())
    {
        pollfd socket_readable;
        socket_readable.events = POLLIN;
        socket_readable.fd = this->test_fd;

        ASSERT_GE(poll(&socket_readable, 1, 10000), 1);
        ASSERT_EQ(0, socket_readable.revents & (POLLERR | POLLHUP));

        auto result =
            read(this->test_fd, received.data() + bytes_read, std::min(received.size() - bytes_read, chunk_size));
        ASSERT_GE(result, 0) << "Failed to read(): " << strerror(errno);
        bytes_read += result;

        pthread_kill(writer.native_handle(), SIGALRM);
        std::this_thread::yield();
        pthread_kill(writer.native_handle(), SIGALRM);
    }

    writer.stop();

    EXPECT_EQ(expected, received);
}

TYPED_TEST(StreamTransportTest, ReadingZeroBytesIsAnError)
{
    EXPECT_THROW(this->transport->receive_data(nullptr, 0),
                 std::logic_error);

    std::vector<mir::Fd> dummy;
    EXPECT_THROW(this->transport->receive_data(nullptr, 0, dummy),
                 std::logic_error);
}

TYPED_TEST(StreamTransportTest, ReceivingDataWithoutAskingForFdsIsAnErrorWhenThereAreFds)
{
    constexpr int num_fds{1};

    std::array<TestFd, num_fds> test_files;
    std::array<int, num_fds> test_fds;
    for (unsigned int i = 0; i < test_fds.size(); ++i)
    {
        test_fds[i] = test_files[i].fd;
    }

    auto receive_done = std::make_shared<mir::test::Signal>();
    mir::test::AutoUnblockThread reader{[this]()
                                        { ::close(this->test_fd); },
                                        [&]()
                                        {
        uint32_t dummy;
        EXPECT_THROW(this->transport->receive_data(&dummy, sizeof(dummy)),
                     std::runtime_error);
        receive_done->raise();
    }};

    int32_t dummy{0};
    EXPECT_EQ(sizeof(dummy), send_with_fds(this->test_fd, test_fds, &dummy, sizeof(dummy), MSG_DONTWAIT));

    EXPECT_TRUE(receive_done->wait_for(std::chrono::seconds{1}));
}
