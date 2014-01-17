/*
 * Copyright (C) 2013 Canonical, Ltd.
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
 *
 * Author: Gerry Boland <gerry.boland@canonical.com>
 * Bits and pieces taken from the QtWayland portion of the Qt project which is
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 */

#include "mirbuffersgtexture.h"

// Mir
#include <mir/graphics/buffer.h>
#include <mir/geometry/size.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
#include <private/qqmlprofilerservice_p.h>
#include <QElapsedTimer>
static QElapsedTimer qsg_renderer_timer;
static bool qsg_render_timing = !qgetenv("QSG_RENDER_TIMING").isEmpty();
#endif

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
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    bool profileFrames = qsg_render_timing || QQmlProfilerService::enabled;
    if (profileFrames)
        qsg_renderer_timer.start();
#endif

    glBindTexture(GL_TEXTURE_2D, m_textureId);
    updateBindOptions(true/* force */);
    m_mirBuffer->bind_to_texture();

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    qint64 bindTime = 0;
    if (profileFrames)
        bindTime = qsg_renderer_timer.nsecsElapsed();

    if (qsg_render_timing) {
        printf("   - mirbuffertexture(%dx%d) bind=%d, total=%d\n",
               m_width, m_height,
               int(bindTime/1000000),
               (int) qsg_renderer_timer.elapsed());
    }

    if (QQmlProfilerService::enabled) {
        QQmlProfilerService::sceneGraphFrame(
                    QQmlProfilerService::SceneGraphTexturePrepare,
                    bindTime);
    }
#endif
}
