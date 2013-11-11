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

    bool isSharing() const override { return true; }

    QFunctionPointer getProcAddress(const QByteArray &procName) override;

private:
    QSurfaceFormat m_format;
    mir::DefaultServerConfiguration *m_mirConfig;
};

#endif // MIROPENGLCONTEXT_H
