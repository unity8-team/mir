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

#include "miropenglcontext.h"

#include "displaywindow.h"
#include "mirserver.h"
#include "mirglconfig.h"

#include <QDebug>

#include <QSurfaceFormat>
#include <QtPlatformSupport/private/qeglconvenience_p.h>

// Mir
#include <mir/graphics/display.h>
#include <mir/graphics/gl_context.h>

// Qt supports one GL context per screen, but also shared contexts.
// The Mir "Display" generates a shared GL context for all DisplayBuffers
// (i.e. individual display output buffers) to use as a common base context.

MirOpenGLContext::MirOpenGLContext(const QSharedPointer<mir::Server> &config, const QSurfaceFormat &format)
    : m_mirConfig(config)
#if GL_DEBUG
    , m_logger(new QOpenGLDebugLogger(this))
#endif
{
    std::shared_ptr<mir::graphics::Display> display = m_mirConfig->the_display();

    // create a temporary GL context to fetch the EGL display and config, so Qt can determine the surface format
    std::unique_ptr<mir::graphics::GLContext> mirContext = display->create_gl_context();
    mirContext->make_current();

    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY) {
        qFatal("Unable to determine current EGL Display");
    }
    EGLContext eglContext = eglGetCurrentContext();
    if (eglContext == EGL_NO_CONTEXT) {
        qFatal("Unable to determine current EGL Context");
    }
    EGLint eglConfigId = -1;
    EGLBoolean result;
    result = eglQueryContext(eglDisplay, eglContext, EGL_CONFIG_ID, &eglConfigId);
    if (result != EGL_TRUE || eglConfigId < 0) {
        qFatal("Unable to determine current EGL Config ID");
    }

    EGLConfig eglConfig;
    EGLint matchingEglConfigCount;
    EGLint const attribList[] = {
        EGL_CONFIG_ID, eglConfigId,
        EGL_NONE
    };
    result = eglChooseConfig(eglDisplay, attribList, &eglConfig, 1, &matchingEglConfigCount);
    if (result != EGL_TRUE || eglConfig == nullptr || matchingEglConfigCount < 1) {
        qFatal("Unable to select EGL Config with the supposed current config ID");
    }

    QSurfaceFormat formatCopy = format;
#ifdef QTMIR_USE_OPENGL
    formatCopy.setRenderableType(QSurfaceFormat::OpenGL);
#else
    formatCopy.setRenderableType(QSurfaceFormat::OpenGLES);
#endif

    m_format = q_glFormatFromConfig(eglDisplay, eglConfig, formatCopy);

    // FIXME: the temporary gl context created by Mir does not have the attributes we specified
    // in the GLConfig, so need to set explicitly for now
    m_format.setDepthBufferSize(config->the_gl_config()->depth_buffer_bits());
    m_format.setStencilBufferSize(config->the_gl_config()->stencil_buffer_bits());
    m_format.setSamples(-1);

#ifndef QT_NO_DEBUG
    const char* string = (const char*) glGetString(GL_VENDOR);
    qDebug() << "OpenGL ES vendor: " << qPrintable(string);
    string = (const char*) glGetString(GL_RENDERER);
    qDebug() << "OpenGL ES renderer"  << qPrintable(string);
    string = (const char*) glGetString(GL_VERSION);
    qDebug() << "OpenGL ES version" << qPrintable(string);
    string = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
    qDebug() << "OpenGL ES Shading Language version:" << qPrintable(string);
    string = (const char*) glGetString(GL_EXTENSIONS);
    qDebug() << "OpenGL ES extensions:" << qPrintable(string);
    q_printEglConfig(eglDisplay, eglConfig);

#if GL_DEBUG
    QObject::connect(m_logger, &QOpenGLDebugLogger::messageLogged,
                     this, &MirOpenGLContext::onGlDebugMessageLogged, Qt::DirectConnection);
#endif // Qt>=5.2
#endif // debug
}

QSurfaceFormat MirOpenGLContext::format() const
{
    return m_format;
}

void MirOpenGLContext::swapBuffers(QPlatformSurface *surface)
{
#ifdef QTMIR_USE_OPENGL
    eglBindAPI(EGL_OPENGL_API);
#endif

    // ultimately calls Mir's DisplayBuffer::post_update()
    DisplayWindow *displayBuffer = static_cast<DisplayWindow*>(surface);
    displayBuffer->swapBuffers(); //blocks for vsync
}

bool MirOpenGLContext::makeCurrent(QPlatformSurface *surface)
{
#ifdef QTMIR_USE_OPENGL
    eglBindAPI(EGL_OPENGL_API);
#endif

    // ultimately calls Mir's DisplayBuffer::make_current()
    DisplayWindow *displayBuffer = static_cast<DisplayWindow*>(surface);
    if (displayBuffer) {
        displayBuffer->makeCurrent();

#if GL_DEBUG
        if (!m_logger->isLogging() && m_logger->initialize()) {
            m_logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
            m_logger->enableMessages();
        }
#endif

        return true;
    }

    return false;
}

void MirOpenGLContext::doneCurrent()
{
    // could call Mir's DisplayBuffer::release_current(), but for what DisplayBuffer?
}

QFunctionPointer MirOpenGLContext::getProcAddress(const QByteArray &procName)
{
#ifdef QTMIR_USE_OPENGL
    eglBindAPI(EGL_OPENGL_API);
#endif

    return eglGetProcAddress(procName.constData());
}
