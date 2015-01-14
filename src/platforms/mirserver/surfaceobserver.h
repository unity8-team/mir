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

#ifndef SESSIONOBSERVER_H
#define SESSIONOBSERVER_H

#include <QObject>
#include <mir/scene/surface_observer.h>

class SurfaceObserver : public QObject, public mir::scene::SurfaceObserver {
    Q_OBJECT

public:
    SurfaceObserver();

    void setListener(QObject *listener);

    void attrib_changed(MirSurfaceAttrib, int) override {}
    void resized_to(mir::geometry::Size const&) override {}
    void moved_to(mir::geometry::Point const&) override {}
    void hidden_set_to(bool) override {}

    // Get new frame notifications from Mir, called from a Mir thread.
    void frame_posted(int frames_available) override;

    void alpha_set_to(float) override {}
    void transformation_set_to(glm::mat4 const&) override {}
    void reception_mode_set_to(mir::input::InputReceptionMode) override {}
    void cursor_image_set_to(mir::graphics::CursorImage const&) override {}
    void orientation_set_to(MirOrientation) override {}
    void client_surface_close_requested() override {}

Q_SIGNALS:
    void framesPosted();

private:
    QObject *m_listener;
    bool m_framesPosted;
};

#endif
