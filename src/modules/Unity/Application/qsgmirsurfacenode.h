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
