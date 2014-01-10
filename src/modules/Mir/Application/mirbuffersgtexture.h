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
 */

#ifndef MIRBUFFERSGTEXTURE_H
#define MIRBUFFERSGTEXTURE_H

#include <memory>

#include <QSGTexture>

#include <QtGui/qopengl.h>

namespace mir { namespace graphics { class Buffer; }}

class MirBufferSGTexture : public QSGTexture
{
    Q_OBJECT
public:
    MirBufferSGTexture(std::shared_ptr<mir::graphics::Buffer>);
    ~MirBufferSGTexture();

    int textureId() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override { return false; }

    void bind() override;

private:
    std::shared_ptr<mir::graphics::Buffer> m_mirBuffer;
    int m_height;
    int m_width;
    GLuint m_textureId;
};

#endif // MIRBUFFERSGTEXTURE_H
