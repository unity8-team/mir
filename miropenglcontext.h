#ifndef MIROPENGLCONTEXT_H
#define MIROPENGLCONTEXT_H

#include <qpa/qplatformopenglcontext.h>

namespace mir { class DefaultServerConfiguration; }

class MirOpenGLContext : public QPlatformOpenGLContext
{
public:
    MirOpenGLContext(mir::DefaultServerConfiguration *, QSurfaceFormat);
    ~MirOpenGLContext();

    QSurfaceFormat format() const override;
    void swapBuffers(QPlatformSurface *surface) override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;

    bool isSharing() const override { return false; }

    QFunctionPointer getProcAddress(const QByteArray &procName) override;

private:
    mir::DefaultServerConfiguration *m_mirConfig;
    QSurfaceFormat m_format;
};

#endif // MIROPENGLCONTEXT_H
