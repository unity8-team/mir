/*
 * Copyright (C) 2014 Canonical, Ltd.
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
 */

#include "surfaceobserver.h"

#include <QMetaObject>

SurfaceObserver::SurfaceObserver()
    : m_listener(nullptr)
    , m_framesPosted(false)
{
}

void SurfaceObserver::setListener(QObject *listener) {
    m_listener = listener;
    if (m_framesPosted) {
        QMetaObject::invokeMethod(m_listener, "surfaceDamaged", Qt::QueuedConnection);
    }
}

bool SurfaceObserver::framesPosted() const
{
    return m_framesPosted;
}

void SurfaceObserver::frame_posted(int /*frames_available*/) {
    m_framesPosted = true;
    if (m_listener) {
        QMetaObject::invokeMethod(m_listener, "surfaceDamaged", Qt::QueuedConnection);
    }
}