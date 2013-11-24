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

#ifndef QSGMIRSURFACENODE_H
#define QSGMIRSURFACENODE_H

#include <QSGSimpleTextureNode>

class MirSurfaceItem;

class QSGMirSurfaceNode : public QSGSimpleTextureNode
{
public:
    QSGMirSurfaceNode();

    QSGMirSurfaceNode(MirSurfaceItem *item = 0);
    ~QSGMirSurfaceNode();

    void preprocess();
    void updateTexture();

    bool isTextureUpdated() const { return m_textureUpdated; }
    void setTextureUpdated(bool textureUpdated) { m_textureUpdated = textureUpdated; }

    MirSurfaceItem *item() const { return m_item; }
    void setItem(MirSurfaceItem *item) { m_item = item; }

private:
    MirSurfaceItem *m_item;
    bool m_textureUpdated;
    bool m_useTextureAlpha;
};

#endif // QSGMIRSURFACENODE_H
