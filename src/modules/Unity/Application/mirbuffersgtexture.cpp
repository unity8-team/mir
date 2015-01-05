/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mirbuffersgtexture.h"

// Mir
#include <mir/graphics/buffer.h>
#include <mir/geometry/size.h>

// QQuickProfiler uses the pretty syntax for emit, signal & slot
#define emit Q_EMIT
#define signals Q_SIGNALS
#define slots Q_SLOTS
#include <private/qquickprofiler_p.h>
#undef emit
#undef signals
#undef slots
#include <QElapsedTimer>
static QElapsedTimer qsg_renderer_timer;
static bool qsg_render_timing = !qgetenv("QSG_RENDER_TIMING").isEmpty();

namespace mg = mir::geometry;

MirBufferSGTexture::MirBufferSGTexture(std::shared_ptr<mir::graphics::Buffer> buffer)
    : QSGTexture()
    , m_mirBuffer(buffer)
    , m_textureId(0)
{
    glGenTextures(1, &m_textureId);

    setFiltering(QSGTexture::Linear);
    setHorizontalWrapMode(QSGTexture::ClampToEdge);
    setVerticalWrapMode(QSGTexture::ClampToEdge);

    mg::Size size = m_mirBuffer->size();
    m_height = size.height.as_int();
    m_width = size.width.as_int();
}

MirBufferSGTexture::~MirBufferSGTexture()
{
    if (m_textureId) {
        glDeleteTextures(1, &m_textureId);
    }
}

void MirBufferSGTexture::freeBuffer()
{
    m_mirBuffer.reset();
}

void MirBufferSGTexture::setBuffer(std::shared_ptr<mir::graphics::Buffer> buffer)
{
    m_mirBuffer = buffer;
    mg::Size size = buffer->size();
    m_height = size.height.as_int();
    m_width = size.width.as_int();
}

int MirBufferSGTexture::textureId() const
{
    return m_textureId;
}

QSize MirBufferSGTexture::textureSize() const
{
    return QSize(m_width, m_height);
}

bool MirBufferSGTexture::hasAlphaChannel() const
{
    return m_mirBuffer->pixel_format() == mir_pixel_format_abgr_8888
        || m_mirBuffer->pixel_format() == mir_pixel_format_argb_8888;
}

void MirBufferSGTexture::bind()
{
#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
    bool profileFrames = qsg_render_timing || QQuickProfiler::enabled;
#else
    bool profileFrames = qsg_render_timing || QQuickProfiler::profilingSceneGraph();
#endif
    if (profileFrames)
        qsg_renderer_timer.start();

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    updateBindOptions(true/* force */);
    m_mirBuffer->gl_bind_to_texture();

    qint64 bindTime = 0;
    if (profileFrames)
        bindTime = qsg_renderer_timer.nsecsElapsed();

    if (qsg_render_timing) {
        printf("   - mirbuffertexture(%dx%d) bind=%d, total=%d\n",
               m_width, m_height,
               int(bindTime/1000000),
               (int) qsg_renderer_timer.elapsed());
    }

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
    Q_QUICK_SG_PROFILE1(QQuickProfiler::SceneGraphTexturePrepare, (
#else
    Q_QUICK_SG_PROFILE(QQuickProfiler::SceneGraphTexturePrepare, (
#endif
            bindTime,  // bind (all this does)
            0,  // convert (not relevant)
            0,  // swizzle (not relevant)
            0,  // upload (not relevant)
            0)); // mipmap (not used ever...)
}
