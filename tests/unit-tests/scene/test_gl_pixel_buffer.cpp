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

#include "src/server/scene/gl_pixel_buffer.h"
#include "mir/graphics/gl_context.h"

#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_gl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <GLES2/gl2ext.h>

namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace ms = mir::scene;
namespace mtd = mir::test::doubles;

namespace
{

struct MockGLContext : public mg::GLContext
{
    ~MockGLContext() noexcept {}
    MOCK_CONST_METHOD0(make_current, void());
    MOCK_CONST_METHOD0(release_current, void());
};

struct WrappingGLContext : public mg::GLContext
{
    WrappingGLContext(mg::GLContext& wrapped)
        : wrapped(wrapped)
    {
    }
    ~WrappingGLContext() noexcept {}
    void make_current() const { wrapped.make_current(); }
    void release_current() const { wrapped.release_current(); }

    mg::GLContext& wrapped;
};

class GLPixelBufferTest : public ::testing::Test
{
public:
    GLPixelBufferTest()
        : context{new WrappingGLContext{mock_context}}
    {
        using namespace testing;

        ON_CALL(mock_buffer, size())
            .WillByDefault(Return(geom::Size{51, 71}));
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

    for (uint32_t i = 0; i < width * height; ++i)
    {
        pixels[i] = i;
    }
}

ACTION(FillPixelsRGBA)
{
    auto const pixels = static_cast<uint32_t*>(arg6);
    size_t const width = arg2;
    size_t const height = arg3;

    for (uint32_t i = 0; i < width * height; ++i)
    {
        pixels[i] = ((i << 16) & 0x00ff0000) | /* Move R to new position */
                    ((i) & 0x0000ff00) |       /* G remains at same position */
                    ((i >> 16) & 0x000000ff) | /* Move B to new position */
                    ((i) & 0xff000000);        /* A remains at same position */
    }
}

}

TEST_F(GLPixelBufferTest, returns_empty_if_not_initialized)
{
    ms::GLPixelBuffer pixels{std::move(context)};

    EXPECT_EQ(geom::Size(), pixels.size());
    EXPECT_EQ(geom::Stride(), pixels.stride());
}

/* specifically, the tegra3 based N7 generates an out of memory error when binding to texture */
TEST_F(GLPixelBufferTest, abort_on_bad_texture_bind)
{
    using namespace testing;
    GLuint const tex{10};
    GLuint const fbo{20};
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

        EXPECT_CALL(mock_buffer, bind_to_texture())
            .Times(1)
            .WillOnce(Throw(std::runtime_error("bad bind.\n")));

        /* recover default FB */
        EXPECT_CALL(mock_gl, glBindFramebuffer(_,0));

        /* do not call these if the bind was not okay */
        EXPECT_CALL(mock_gl, glFramebufferTexture2D(_,_,_,_,_))
            .Times(0);
        EXPECT_CALL(mock_gl, glReadPixels(_,_,_,_,_,_,_))
            .Times(0);
    }

    ms::GLPixelBuffer pixels{std::move(context)};

    geom::Size bkp_size{1,1};
    geom::Stride bkp_stride{4};
    unsigned int pixel_color = 0xFF33FF33;

    EXPECT_THROW({
        pixels.fill_from(mock_buffer);
    }, std::runtime_error);
    auto data = pixels.as_argb_8888();
    
    EXPECT_EQ(bkp_size, pixels.size());
    EXPECT_EQ(bkp_stride, pixels.stride());
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(pixel_color, static_cast<uint32_t const*>(data)[0]);
}

TEST_F(GLPixelBufferTest, unable_to_bind_fb_results_in_dark_green_pixel)
{
    using namespace testing;
    GLuint const tex{10};
    GLuint const fbo{20};
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
        EXPECT_CALL(mock_buffer, bind_to_texture());
        EXPECT_CALL(mock_gl, glFramebufferTexture2D(_,_,_,_,_));
        EXPECT_CALL(mock_gl, glCheckFramebufferStatus(GL_FRAMEBUFFER))
            .Times(1)
            .WillOnce(Return(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT));

        /* recover default FB */
        EXPECT_CALL(mock_gl, glBindFramebuffer(_,0));

        /* do not try to read if the fbo status is not okay */
        EXPECT_CALL(mock_gl, glReadPixels(_,_,_,_,_,_,_))
            .Times(0);
    }

    ms::GLPixelBuffer pixels{std::move(context)};

    geom::Size bkp_size{1,1};
    geom::Stride bkp_stride{4};
    unsigned int pixel_color = 0xFF33FF33;

    EXPECT_THROW({
        pixels.fill_from(mock_buffer);
    }, std::runtime_error);
    auto data = pixels.as_argb_8888();

    
    EXPECT_EQ(bkp_size, pixels.size());
    EXPECT_EQ(bkp_stride, pixels.stride());
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(pixel_color, static_cast<uint32_t const*>(data)[0]);
}

TEST_F(GLPixelBufferTest, returns_data_from_bgra_buffer_texture)
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

        /* The buffer texture is read as BGRA */
        EXPECT_CALL(mock_buffer, bind_to_texture());
        EXPECT_CALL(mock_gl, glFramebufferTexture2D(_,_,_,tex,0));
        EXPECT_CALL(mock_gl, glCheckFramebufferStatus(GL_FRAMEBUFFER))
            .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE));
        EXPECT_CALL(mock_gl, glReadPixels(0, 0, width, height,
                                          GL_BGRA_EXT, GL_UNSIGNED_BYTE, _))
            .WillOnce(FillPixels());
    }

    ms::GLPixelBuffer pixels{std::move(context)};

    pixels.fill_from(mock_buffer);
    auto data = pixels.as_argb_8888();

    EXPECT_EQ(mock_buffer.size(), pixels.size());
    EXPECT_EQ(geom::Stride{width * 4}, pixels.stride());

    /* Check that data has been properly y-flipped */
    EXPECT_EQ(1,
              static_cast<uint32_t const*>(data)[width * (height - 1) + 1]);
    EXPECT_EQ(width * (height / 2),
              static_cast<uint32_t const*>(data)[width * (height / 2)]);
    EXPECT_EQ(width * (height - 1),
              static_cast<uint32_t const*>(data)[0]);
    EXPECT_EQ(width - 1,
              static_cast<uint32_t const*>(data)[width * height - 1]);
}

TEST_F(GLPixelBufferTest, returns_data_from_rgba_buffer_texture)
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

        /* Try to read the FBO as BGRA but fail */
        EXPECT_CALL(mock_buffer, bind_to_texture());
        EXPECT_CALL(mock_gl, glFramebufferTexture2D(_,_,_,tex,0));
        EXPECT_CALL(mock_gl, glCheckFramebufferStatus(GL_FRAMEBUFFER))
            .Times(1)
            .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE));

        EXPECT_CALL(mock_gl, glGetError())
            .WillOnce(Return(GL_NO_ERROR));
        EXPECT_CALL(mock_gl, glReadPixels(0, 0, width, height,
                                          GL_BGRA_EXT, GL_UNSIGNED_BYTE, _));
        EXPECT_CALL(mock_gl, glGetError())
            .WillOnce(Return(GL_INVALID_ENUM));
        /* Read as RGBA */
        EXPECT_CALL(mock_gl, glReadPixels(0, 0, width, height,
                                          GL_RGBA, GL_UNSIGNED_BYTE, _))
            .WillOnce(FillPixelsRGBA());
    }

    ms::GLPixelBuffer pixels{std::move(context)};

    pixels.fill_from(mock_buffer);
    auto data = pixels.as_argb_8888();

    EXPECT_EQ(mock_buffer.size(), pixels.size());
    EXPECT_EQ(geom::Stride{width * 4}, pixels.stride());

    /* Check that data has been properly y-flipped and converted to argb_8888 */
    EXPECT_EQ(1,
              static_cast<uint32_t const*>(data)[width * (height - 1) + 1]);
    EXPECT_EQ(width * (height / 2),
              static_cast<uint32_t const*>(data)[width * (height / 2)]);
    EXPECT_EQ(width * (height - 1),
              static_cast<uint32_t const*>(data)[0]);
    EXPECT_EQ(width - 1,
              static_cast<uint32_t const*>(data)[width * height - 1]);
}
