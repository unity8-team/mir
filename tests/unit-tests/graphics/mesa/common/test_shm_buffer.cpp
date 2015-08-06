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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/platforms/mesa/server/common/shm_buffer.h"
#include "src/platforms/mesa/server/common/shm_file.h"

#include "mir/test/doubles/mock_gl.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <GLES2/gl2ext.h>
#include <endian.h>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;
using namespace testing;

namespace
{

struct StubShmFile : public mgm::ShmFile
{
    void* base_ptr() const { return fake_mapping; }
    int fd() const { return fake_fd; }

    void* const fake_mapping = reinterpret_cast<void*>(0x12345678);
    int const fake_fd = 17;
};

struct ShmBufferTest : public testing::Test
{
    ShmBufferTest()
        : size{150,340},
          pixel_format{mir_pixel_format_bgr_888},
          stub_shm_file{std::make_shared<StubShmFile>()},
          shm_buffer{stub_shm_file, size, pixel_format}
    {
    }

    geom::Size const size;
    MirPixelFormat const pixel_format;
    std::shared_ptr<StubShmFile> const stub_shm_file;
    mgm::ShmBuffer shm_buffer;
    testing::NiceMock<mtd::MockGL> mock_gl;
};

}

TEST_F(ShmBufferTest, has_correct_properties)
{
    size_t const bytes_per_pixel = MIR_BYTES_PER_PIXEL(pixel_format);
    size_t const expected_stride{bytes_per_pixel * size.width.as_uint32_t()};

    EXPECT_EQ(size, shm_buffer.size());
    EXPECT_EQ(geom::Stride{expected_stride}, shm_buffer.stride());
    EXPECT_EQ(pixel_format, shm_buffer.pixel_format());
}

TEST_F(ShmBufferTest, native_buffer_contains_correct_data)
{
    size_t const bytes_per_pixel = MIR_BYTES_PER_PIXEL(pixel_format);
    size_t const expected_stride{bytes_per_pixel * size.width.as_uint32_t()};

    auto native_buffer = shm_buffer.native_buffer_handle();

    EXPECT_EQ(1, native_buffer->fd_items);
    EXPECT_EQ(stub_shm_file->fake_fd, native_buffer->fd[0]);

    EXPECT_EQ(size.width.as_int(), native_buffer->width);
    EXPECT_EQ(size.height.as_int(), native_buffer->height);
    EXPECT_EQ(expected_stride, static_cast<size_t>(native_buffer->stride));
}

TEST_F(ShmBufferTest, cannot_be_used_for_bypass)
{
    EXPECT_FALSE(shm_buffer.native_buffer_handle()->flags & mir_buffer_flag_can_scanout);
}

TEST_F(ShmBufferTest, cant_upload_bgr_888)
{
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, _,
                                      size.width.as_int(), size.height.as_int(),
                                      0, _, _,
                                      stub_shm_file->fake_mapping))
                .Times(0);

    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_bgr_888);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_rgb_888_correctly)
{
    EXPECT_CALL(mock_gl, glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGB, GL_UNSIGNED_BYTE,
                                      stub_shm_file->fake_mapping));

    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_rgb_888);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_rgb_565_correctly)
{
    EXPECT_CALL(mock_gl, glPixelStorei(GL_UNPACK_ALIGNMENT,
                                       AnyOf(Eq(1),Eq(2))));
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                                      stub_shm_file->fake_mapping));

    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_rgb_565);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_rgba_5551_correctly)
{
    EXPECT_CALL(mock_gl, glPixelStorei(GL_UNPACK_ALIGNMENT,
                                       AnyOf(Eq(1),Eq(2))));
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1,
                                      stub_shm_file->fake_mapping));

    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_rgba_5551);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_rgba_4444_correctly)
{
    EXPECT_CALL(mock_gl, glPixelStorei(GL_UNPACK_ALIGNMENT,
                                       AnyOf(Eq(1),Eq(2))));
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4,
                                      stub_shm_file->fake_mapping));

    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_rgba_4444);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_xrgb_8888_correctly)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                                      stub_shm_file->fake_mapping));
#endif
    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_xrgb_8888);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_argb_8888_correctly)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                                      stub_shm_file->fake_mapping));
#endif
    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_argb_8888);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_xbgr_8888_correctly)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGBA, GL_UNSIGNED_BYTE,
                                      stub_shm_file->fake_mapping));
#endif
    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_xbgr_8888);
    buf.gl_bind_to_texture();
}

TEST_F(ShmBufferTest, uploads_abgr_8888_correctly)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    EXPECT_CALL(mock_gl, glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                                      size.width.as_int(), size.height.as_int(),
                                      0, GL_RGBA, GL_UNSIGNED_BYTE,
                                      stub_shm_file->fake_mapping));
#endif
    mgm::ShmBuffer buf(stub_shm_file, size, mir_pixel_format_abgr_8888);
    buf.gl_bind_to_texture();
}
