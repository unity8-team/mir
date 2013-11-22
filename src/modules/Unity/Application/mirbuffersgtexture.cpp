#include "mirbuffersgtexture.h"

// Mir
#include <mir/graphics/buffer.h>
#include <mir/geometry/pixel_format.h>
#include <mir/geometry/size.h>

namespace mg = mir::geometry;

MirBufferSGTexture::MirBufferSGTexture(std::shared_ptr<mir::graphics::Buffer> buffer)
    : QSGTexture()
    , m_mirBuffer(buffer)
{
    setFiltering(QSGTexture::Linear);
    setHorizontalWrapMode(QSGTexture::ClampToEdge);
    setVerticalWrapMode(QSGTexture::ClampToEdge);
    updateBindOptions();
}

MirBufferSGTexture::~MirBufferSGTexture()
{
}

int MirBufferSGTexture::textureId() const
{
    // is this needed?? Try fake value for now.
    return 1;
}

QSize MirBufferSGTexture::textureSize() const
{
    mg::Size size = m_mirBuffer->size();
    return QSize(size.width.as_int(), size.height.as_int());
}

bool MirBufferSGTexture::hasAlphaChannel() const
{
    return mg::has_alpha(m_mirBuffer->pixel_format());
}

void MirBufferSGTexture::bind()
{
    m_mirBuffer->bind_to_texture();
}
