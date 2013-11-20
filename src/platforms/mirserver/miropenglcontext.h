#ifndef MIROPENGLCONTEXT_H
#define MIROPENGLCONTEXT_H

#include <qpa/qplatformopenglcontext.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
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

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0) && !defined(QT_NO_DEBUG)
    Q_SLOT void onGlDebugMessageLogged(QOpenGLDebugMessage m) { qDebug() << m; }
#endif

private:
    mir::DefaultServerConfiguration *m_mirConfig;
    QSurfaceFormat m_format;
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    QOpenGLDebugLogger *m_logger;
#endif
};

#endif // MIROPENGLCONTEXT_H
