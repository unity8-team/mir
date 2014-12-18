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
 */

#ifndef MIROPENGLCONTEXT_H
#define MIROPENGLCONTEXT_H

#include <qpa/qplatformopenglcontext.h>

#define GL_DEBUG (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0) && !defined(QT_NO_DEBUG))

#if GL_DEBUG
#include <QOpenGLDebugLogger>
#endif

class MirServer;

class MirOpenGLContext : public QObject, public QPlatformOpenGLContext
{
    Q_OBJECT
public:
    MirOpenGLContext(const QSharedPointer<MirServer> &, const QSurfaceFormat &);
    ~MirOpenGLContext() = default;

    QSurfaceFormat format() const override;
    void swapBuffers(QPlatformSurface *surface) override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;

    bool isSharing() const override { return false; }

    QFunctionPointer getProcAddress(const QByteArray &procName) override;

// "#if GL_DEBUG" does not work as MOC does not understand #define
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0) && !defined(QT_NO_DEBUG))
    Q_SLOT void onGlDebugMessageLogged(QOpenGLDebugMessage m) { qDebug() << m; }
#endif

private:
    const QSharedPointer<MirServer> m_mirServer;
    QSurfaceFormat m_format;
#if GL_DEBUG
    QOpenGLDebugLogger *m_logger;
#endif
};

#endif // MIROPENGLCONTEXT_H
