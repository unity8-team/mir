/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/test/doubles/mock_protobuf_server.h"
#include "src/client/presentation_chain.h"
#include "mir/client_buffer_factory.h"

#include <mutex>
#include <condition_variable>
#include <gtest/gtest.h>
using namespace testing;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;
namespace mcl = mir::client;
namespace mp = mir::protobuf;
namespace gp = google::protobuf;

namespace
{
struct MockClientBufferFactory : public mcl::ClientBufferFactory
{
    MockClientBufferFactory()
    {
        ON_CALL(*this, create_buffer(_,_,_))
            .WillByDefault(Return(nullptr));
    }
    MOCK_METHOD3(create_buffer, std::shared_ptr<mcl::ClientBuffer>(
        std::shared_ptr<MirBufferPackage> const&, geom::Size, MirPixelFormat));
};

struct PresentationChain : Test
{
    PresentationChain()
    {
        ipc_buf.set_width(size.width.as_int());
        ipc_buf.set_height(size.height.as_int());
        ipc_buf.set_buffer_id(buffer_id);
    }

    int rpc_id { 33 };
    MirConnection* connection {reinterpret_cast<MirConnection*>(this)};
    geom::Size size {100, 200};
    MirPixelFormat format = mir_pixel_format_abgr_8888;
    MirBufferUsage usage = mir_buffer_usage_software;
    mtd::MockProtobufServer mock_server;
    std::shared_ptr<MockClientBufferFactory> const factory {
        std::make_shared<NiceMock<MockClientBufferFactory>>() };
    int buffer_id {4312};
    mp::Buffer ipc_buf;
};

struct BufferCallbackContext
{
    void set_buffer(MirBuffer* b)
    {
        std::unique_lock<std::mutex> lk(mut);
        buffer = b;
        cv.notify_all();
    }

    bool buffer_is_set()
    {
        std::unique_lock<std::mutex> lk(mut);
        return buffer;
    }

    MirBuffer* wait_for_buffer()
    {
        std::unique_lock<std::mutex> lk(mut);
        if (!cv.wait_for(lk, std::chrono::seconds(5), [this] { return buffer; }))
            throw std::runtime_error("timeout waiting for buffer");
        return buffer;
    }

private:
    std::mutex mut;
    std::condition_variable cv;
    MirBuffer* buffer = nullptr;
};

struct BufferCount
{
    std::mutex mut;
    std::condition_variable cv;
    MirBuffer* buffer = nullptr;
    unsigned int count = 0;
};

MATCHER_P(BufferRequestMatches, val, "")
{
    return ((arg->id().value() == val.id().value()) &&
        arg->has_buffer() &&
        val.has_buffer() &&
        arg->buffer().buffer_id() == val.buffer().buffer_id());
}

MATCHER_P(BufferAllocationMatches, val, "")
{
    return ((arg->id().value() == val.id().value()) &&
        (arg->buffer_requests_size() == 1) &&
        (val.buffer_requests_size() == 1) &&
        (arg->buffer_requests(0).width() == val.buffer_requests(0).width()) &&
        (arg->buffer_requests(0).height() == val.buffer_requests(0).height()) &&
        (arg->buffer_requests(0).pixel_format() == val.buffer_requests(0).pixel_format()) &&
        (arg->buffer_requests(0).buffer_usage() == val.buffer_requests(0).buffer_usage()));
}

MATCHER_P(BufferReleaseMatches, val, "")
{
    return ((arg->id().value() == val.id().value()) &&
        (arg->buffers_size() == 1) &&
        (val.buffers_size() == 1) &&
        (arg->buffers(0).buffer_id() == val.buffers(0).buffer_id()));
}
}

static void buffer_callback(MirPresentationChain*, MirBuffer* buffer, void* context)
{
    static_cast<BufferCallbackContext*>(context)->set_buffer(buffer);
}

static void counting_buffer_callback(MirPresentationChain*, MirBuffer* buffer, void* context)
{
    BufferCount* c = static_cast<BufferCount*>(context);
    c->buffer = buffer;
    c->count = c->count + 1;
}

TEST_F(PresentationChain, returns_associated_connection)
{
    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    EXPECT_THAT(chain.connection(), Eq(connection));
}

TEST_F(PresentationChain, returns_associated_rpc_id)
{
    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    EXPECT_THAT(chain.rpc_id(), Eq(rpc_id));
}

TEST_F(PresentationChain, creates_buffer_when_asked)
{
    BufferCallbackContext buffer;
    mp::BufferAllocation mp_alloc;
    auto params = mp_alloc.add_buffer_requests();
    params->set_width(size.width.as_int());
    params->set_height(size.height.as_int());
    params->set_buffer_usage(usage);
    params->set_pixel_format(format);
    mp_alloc.mutable_id()->set_value(rpc_id);

    EXPECT_CALL(mock_server, allocate_buffers(BufferAllocationMatches(mp_alloc),_,_))
        .WillOnce(mtd::RunProtobufClosure());
    EXPECT_CALL(*factory, create_buffer(_, size, format));
 
    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    chain.allocate_buffer(size, format, usage, buffer_callback, &buffer);

    EXPECT_FALSE(buffer.buffer_is_set());

    mp::Buffer ipc_buf;
    ipc_buf.set_width(size.width.as_int());
    ipc_buf.set_height(size.height.as_int());
    chain.buffer_available(ipc_buf);

    EXPECT_TRUE(buffer.wait_for_buffer());
}

TEST_F(PresentationChain, creates_correct_buffer_when_buffers_arrive)
{
    size_t const num_buffers = 3u;
    std::array<geom::Size, num_buffers> sizes { {
        geom::Size{2,2},
        geom::Size{2,1},
        geom::Size{1,2}
    } };

    std::array<mp::Buffer, num_buffers> ipc_buf;
    std::array<BufferCallbackContext, num_buffers> buffer;
    std::array<mp::BufferAllocation, num_buffers> mp_alloc;
    Sequence seq;
    for (auto i = 0u; i < num_buffers; i++)
    {
        mp_alloc[i].mutable_id()->set_value(rpc_id);
        auto params = mp_alloc[i].add_buffer_requests();
        params->set_width(sizes[i].width.as_int());
        params->set_height(sizes[i].height.as_int());
        params->set_buffer_usage(usage);
        params->set_pixel_format(format);

        EXPECT_CALL(mock_server, allocate_buffers(BufferAllocationMatches(mp_alloc[i]),_,_))
            .InSequence(seq)
            .WillOnce(mtd::RunProtobufClosure());

        ipc_buf[i].set_buffer_id(i);
        ipc_buf[i].set_width(sizes[i].width.as_int());
        ipc_buf[i].set_height(sizes[i].height.as_int());
    }

    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);

    for (auto i = 0u; i < num_buffers; i++)
        chain.allocate_buffer(sizes[i], format, usage, buffer_callback, &buffer[i]);

    chain.buffer_available(ipc_buf[1]);
    EXPECT_FALSE(buffer[0].buffer_is_set());
    EXPECT_TRUE(buffer[1].wait_for_buffer());
    EXPECT_FALSE(buffer[2].buffer_is_set());

    chain.buffer_available(ipc_buf[2]);
    EXPECT_FALSE(buffer[0].buffer_is_set());
    EXPECT_TRUE(buffer[2].wait_for_buffer());

    chain.buffer_available(ipc_buf[0]);
    EXPECT_TRUE(buffer[0].wait_for_buffer());
}

TEST_F(PresentationChain, frees_buffer_when_asked)
{
    BufferCallbackContext buffer;
    mp::BufferRelease release_msg;
    release_msg.mutable_id()->set_value(rpc_id);
    auto released_buffer = release_msg.add_buffers();
    released_buffer->set_buffer_id(buffer_id);

    EXPECT_CALL(mock_server, allocate_buffers(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());
    EXPECT_CALL(mock_server, release_buffers(BufferReleaseMatches(release_msg),_,_))
        .WillOnce(mtd::RunProtobufClosure());

    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    chain.allocate_buffer(size, format, usage, buffer_callback, &buffer);
    chain.buffer_available(ipc_buf);
    auto b = buffer.wait_for_buffer();
    ASSERT_THAT(b, Ne(nullptr));

    chain.release_buffer(b);

} 

TEST_F(PresentationChain, submits_buffer_when_asked)
{
    BufferCallbackContext buffer;
    mp::BufferRequest request;
    request.mutable_id()->set_value(rpc_id);
    request.mutable_buffer()->set_buffer_id(buffer_id);

    EXPECT_CALL(mock_server, allocate_buffers(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());
    EXPECT_CALL(mock_server, submit_buffer(BufferRequestMatches(request),_,_))
        .WillOnce(mtd::RunProtobufClosure());

    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    chain.allocate_buffer(size, format, usage, buffer_callback, &buffer);
    chain.buffer_available(ipc_buf);
    auto b = buffer.wait_for_buffer();
    ASSERT_THAT(b, Ne(nullptr));

    chain.submit_buffer(b);
} 

TEST_F(PresentationChain, double_submission_throws)
{
    BufferCallbackContext buffer;

    EXPECT_CALL(mock_server, allocate_buffers(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());
    EXPECT_CALL(mock_server, submit_buffer(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());

    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    chain.allocate_buffer(size, format, usage, buffer_callback, &buffer);
    chain.buffer_available(ipc_buf);
    auto b = buffer.wait_for_buffer();
    ASSERT_THAT(b, Ne(nullptr));

    chain.submit_buffer(b);
    EXPECT_THROW({
        chain.submit_buffer(b);
    }, std::logic_error);
}

TEST_F(PresentationChain, callback_invoked_when_buffer_returned_from_allocation_and_submission)
{
    BufferCount counter;

    EXPECT_CALL(mock_server, allocate_buffers(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());
    EXPECT_CALL(mock_server, submit_buffer(_,_,_))
        .WillOnce(mtd::RunProtobufClosure());

    mcl::PresentationChain chain(connection, rpc_id, mock_server, factory);
    chain.allocate_buffer(size, format, usage, counting_buffer_callback, &counter);
    chain.buffer_available(ipc_buf);
    std::unique_lock<std::mutex> lk(counter.mut);
    EXPECT_TRUE(counter.cv.wait_for(lk, std::chrono::seconds(5), [&] { return counter.buffer; }));
    lk.unlock();

    chain.submit_buffer(counter.buffer);
    chain.buffer_available(ipc_buf);

    lk.lock();
    EXPECT_THAT(counter.count, Eq(2));
}
