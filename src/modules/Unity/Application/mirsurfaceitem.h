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
 */

#ifndef MIRSURFACEITEM_H
#define MIRSURFACEITEM_H

#include <memory>

// Qt
#include <QMutex>
#include <QSet>
#include <QQuickItem>

// mir
#include <mir/shell/surface.h>
#include <mir_toolkit/common.h>

class MirSurfaceManager;
class QSGMirSurfaceNode;
class QMirSurfaceTextureProvider;
class Application;

class MirSurfaceItem : public QQuickItem
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_ENUMS(State)

    Q_PROPERTY(qreal implicitWidth READ implicitWidth NOTIFY implicitWidthChanged)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight NOTIFY implicitHeightChanged)

    Q_PROPERTY(Type type READ type NOTIFY typeChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool surfaceValid READ surfaceValid NOTIFY surfaceValidChanged)

public:
    explicit MirSurfaceItem(std::shared_ptr<mir::shell::Surface> surface, Application* application, QQuickItem *parent = 0);
    ~MirSurfaceItem();

    enum Type {
        Normal = mir_surface_type_normal,
        Utility = mir_surface_type_utility,
        Dialog = mir_surface_type_dialog,
        Overlay = mir_surface_type_overlay,
        Freestyle = mir_surface_type_freestyle,
        Popover = mir_surface_type_popover,
        };

    enum State {
        Unknown = mir_surface_state_unknown,
        Restored = mir_surface_state_restored,
        Minimized = mir_surface_state_minimized,
        Maximized = mir_surface_state_maximized,
        VertMaximized = mir_surface_state_vertmaximized,
        /* SemiMaximized = mir_surface_state_semimaximized, // see mircommon/mir_toolbox/common.h*/
        Fullscreen = mir_surface_state_fullscreen,
    };

    //getters
    Application* application() const;
    Type type() const;
    State state() const;
    QString name() const;
    bool surfaceValid() const { return m_surfaceValid; }

    // Item surface/texture management
    bool isTextureProvider() const { return true; }
    QSGTextureProvider *textureProvider() const;
    void setDamagedFlag(bool on);

    // QQuickItem overrides
    qreal implicitWidth() const; //override;
    qreal implicitHeight() const; //override;

Q_SIGNALS:
    void typeChanged();
    void stateChanged();
    void nameChanged();
    void surfaceDestroyed();
    void surfaceValidChanged();

    void textureChanged();

    void implicitWidthChanged();
    void implicitHeightChanged();

private slots:
    void surfaceDamaged(const QRect &);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    void touchEvent(QTouchEvent *event) override;

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

private:
    void updateTexture();
    void ensureProvider();

    void setType(const Type&);
    void setState(const State&);

    // called by MirSurfaceManager
    void setAttribute(const MirSurfaceAttrib, const int);
    void setSurfaceValid(const bool);

    static QMutex *mutex;

    std::shared_ptr<mir::shell::Surface> m_surface;
    Application* m_application;
    bool m_damaged;
    bool m_surfaceValid;

    QMirSurfaceTextureProvider *m_provider;
    QSGMirSurfaceNode *m_node;

    friend class QSGMirSurfaceNode;
    friend class MirSurfaceManager;
};

Q_DECLARE_METATYPE(MirSurfaceItem*)

#endif // MIRSURFACEITEM_H
