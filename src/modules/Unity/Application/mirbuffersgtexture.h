#ifndef MIRBUFFERSGTEXTURE_H
#define MIRBUFFERSGTEXTURE_H

#include <memory>

#include <QSGTexture>

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
};

#endif // MIRBUFFERSGTEXTURE_H
