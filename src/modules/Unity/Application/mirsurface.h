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

#ifndef MIRSURFACE_H
#define MIRSURFACE_H

// Qt
#include <QQuickItem>
#include <QSet>

// mir
#include <mir/shell/surface.h>
#include <mir_toolkit/common.h>

namespace mir { namespace geometry { struct Rectangle; }}
class MirSurfaceManager;
class InputArea;
class Application;

class MirSurface : public QQuickItem
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_ENUMS(State)

    Q_PROPERTY(qreal x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(qreal width READ width WRITE setWidth NOTIFY xChanged)
    Q_PROPERTY(qreal height READ height WRITE setHeight NOTIFY xChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)

    Q_PROPERTY(Type type READ type NOTIFY typeChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)

public:
    explicit MirSurface(std::shared_ptr<mir::shell::Surface> surface, Application* application, QQuickItem *parent = 0);
    ~MirSurface();

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

    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal height() const;
    bool isVisible() const;

    //setters
    void setX(qreal);
    void setY(qreal);
    void setWidth(qreal);
    void setHeight(qreal);
    void setVisible(bool);

    void setAttribute(const MirSurfaceAttrib, const int);

Q_SIGNALS:
    void typeChanged();
    void stateChanged();
    void nameChanged();

    void xChanged();
    void yChanged();
    void widthChanged();
    void heightChanged();
    void visibleChanged();

protected:
    std::shared_ptr<mir::shell::Surface> m_surface;

private:
    // start of methods used by InputArea
    void installInputArea(const InputArea* area);
    bool removeInputArea(const InputArea* area);
    bool enableInputArea(const InputArea* area, bool enable = true);
    // end of methods used by InputArea

    void updateMirInputRegion();
    void setType(const Type&);
    void setState(const State&);

    bool disableMirInputArea(const mir::geometry::Rectangle& rect);

    QSet<const InputArea*> m_inputAreas;

    bool m_visible = true; //FIXME(greyback) state should be in Mir::Shell::Surface, not here

    Application* m_application; 

    friend class MirSurfaceManager;
    friend class InputArea;
};

Q_DECLARE_METATYPE(MirSurface*)

#endif // MIRSURFACE_H
