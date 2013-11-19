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
