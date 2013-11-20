#ifndef MIROPENGLCONTEXT_H
#define MIROPENGLCONTEXT_H

#include <qpa/qplatformopenglcontext.h>

#define GL_DEBUG (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0) && !defined(QT_NO_DEBUG))

#if GL_DEBUG
#include <QOpenGLDebugLogger>
#endif

namespace mir { class DefaultServerConfiguration; }

class MirOpenGLContext : public QObject, public QPlatformOpenGLContext
{
    Q_OBJECT
public:
    MirOpenGLContext(mir::DefaultServerConfiguration *, QSurfaceFormat);
    ~MirOpenGLContext();

    QSurfaceFormat format() const override;
    void swapBuffers(QPlatformSurface *surface) override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;

    bool isSharing() const override { return false; }

    QFunctionPointer getProcAddress(const QByteArray &procName) override;

#if GL_DEBUG
    Q_SLOT void onGlDebugMessageLogged(QOpenGLDebugMessage m) { qDebug() << m; }
#endif

private:
    mir::DefaultServerConfiguration *m_mirConfig;
    QSurfaceFormat m_format;
#if GL_DEBUG
    QOpenGLDebugLogger *m_logger;
#endif
};

#endif // MIROPENGLCONTEXT_H
