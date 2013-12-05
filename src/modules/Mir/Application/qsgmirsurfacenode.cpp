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

#include "qsgmirsurfacenode.h"

// Qt
#include <QMutexLocker>
#include <QSGTextureProvider>

// local
#include "mirsurfaceitem.h"

QSGMirSurfaceNode::QSGMirSurfaceNode(MirSurfaceItem *item)
    : m_item(item)
    , m_textureUpdated(false)
    , m_useTextureAlpha(false)
{
    if (m_item)
        m_item->m_node = this;
    setFlag(UsePreprocess,true);
}

QSGMirSurfaceNode::~QSGMirSurfaceNode()
{
    QMutexLocker locker(MirSurfaceItem::mutex);
    if (m_item)
        m_item->m_node = 0;
}

void QSGMirSurfaceNode::preprocess()
{
    QMutexLocker locker(MirSurfaceItem::mutex);

    if (m_item) {
        //Update if the item is dirty and we haven't done an updateTexture for this frame
        if (m_item->m_damaged && !m_textureUpdated) {
            m_item->updateTexture();
            updateTexture();
        }
    }
    //Reset value for next frame: we have not done updatePaintNode yet
    m_textureUpdated = false;
}

void QSGMirSurfaceNode::updateTexture()
{
    Q_ASSERT(m_item && m_item->textureProvider());
    QSGTexture *texture = m_item->textureProvider()->texture();
    setTexture(texture);
}
