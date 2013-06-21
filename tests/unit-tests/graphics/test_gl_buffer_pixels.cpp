/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir/graphics/gl_buffer_pixels.h"
#include "mir/graphics/gl_context.h"

#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_gl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <GLES2/gl2ext.h>

namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;

struct MockGLContext : public mg::GLContext
{
    MOCK_METHOD0(make_current, void());
    MOCK_METHOD0(release_current, void());
};

struct WrappingGLContext : public mg::GLContext
{
    WrappingGLContext(mg::GLContext& wrapped)
        : wrapped(wrapped)
    {
    }
    void make_current() { wrapped.make_current(); }
    void release_current() { wrapped.release_current(); }

    mg::GLContext& wrapped;
};

class GLBufferPixelsTest : public ::testing::Test
{
public:
    GLBufferPixelsTest()
        : context{new WrappingGLContext{mock_context}}
    {
        using namespace testing;

        ON_CALL(mock_buffer, size())
            .WillByDefault(Return(geom::Size{geom::Width{100},
                                             geom::Height{100}}));

    }

    testing::NiceMock<mtd::MockGL> mock_gl;
    testing::NiceMock<mtd::MockBuffer> mock_buffer;
    MockGLContext mock_context;
    std::unique_ptr<WrappingGLContext> context;
};

ACTION(FillPixels)
{
    auto const pixels = static_cast<uint32_t*>(arg6);
    size_t const width = arg2;
    size_t const height = arg3;

    for (size_t i = 0; i < width * height; ++i)
    {
        pixels[i] = i;
    }
}

TEST_F(GLBufferPixelsTest, returns_empty_data_if_not_initialized)
{
    mg::GLBufferPixels pixels{std::move(context)};
    auto data = pixels.as_argb_8888();

    EXPECT_EQ(nullptr, data.pixels);
    EXPECT_EQ(geom::Size(), data.size);
    EXPECT_EQ(geom::Stride(), data.stride);
}

TEST_F(GLBufferPixelsTest, returns_data_from_buffer_texture)
{
    using namespace testing;
    GLuint const tex{10};
    GLuint const fbo{20};
    uint32_t const width{mock_buffer.size().width.as_uint32_t()};
    uint32_t const height{mock_buffer.size().height.as_uint32_t()};

    {
        InSequence s;

        /* The GL context is made current */
        EXPECT_CALL(mock_context, make_current());

        /* The texture and framebuffer are prepared */
        EXPECT_CALL(mock_gl, glGenTextures(_,_))
            .WillOnce(SetArgPointee<1>(tex));
        EXPECT_CALL(mock_gl, glBindTexture(_,tex));
        EXPECT_CALL(mock_gl, glGenFramebuffers(_,_))
            .WillOnce(SetArgPointee<1>(fbo));
        EXPECT_CALL(mock_gl, glBindFramebuffer(_,fbo));

        /* The buffer texture is read as RGBA */
        EXPECT_CALL(mock_buffer, bind_to_texture());
        EXPECT_CALL(mock_gl, glFramebufferTexture2D(_,_,_,tex,0));
        EXPECT_CALL(mock_gl, glReadPixels(0, 0, width, height,
                                          GL_BGRA_EXT, GL_UNSIGNED_BYTE, _))
            .WillOnce(FillPixels());
    }

    mg::GLBufferPixels pixels{std::move(context)};

    pixels.extract_from(mock_buffer);
    auto data = pixels.as_argb_8888();

    EXPECT_EQ(mock_buffer.size(), data.size);
    EXPECT_EQ(geom::Stride{width * 4}, data.stride);

    /* Check data data has been properly y-flipped */
    EXPECT_EQ(width * (height - 1),
              static_cast<uint32_t*>(data.pixels)[0]);
    EXPECT_EQ(width - 1,
              static_cast<uint32_t*>(data.pixels)[width * height - 1]);
}
