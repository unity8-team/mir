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

#ifndef INPUTFILTERAREA_H
#define INPUTFILTERAREA_H

#include "inputarea.h"

class MirSurface;

/*
  Mimic the API provided by the SurfaceFlinger backend of the shell,
  in order to avoid unnecessary code branches in Shell's QML files. The API doesn't
  really do what it says, so it can be a bit confusing, but nevermind.

  Once we no longer desire to have Unity shell working with both SurfaceFlinger and Mir
  "backends", we can remove this file and make shell's QML code use ShellInputArea instead.

 */
class InputFilterArea : public InputArea
{
    Q_OBJECT
    Q_PROPERTY(bool blockInput READ blockInput WRITE setBlockInput NOTIFY blockInputChanged)

public:
    explicit InputFilterArea(QQuickItem *parent = 0);

    virtual void setSurface(MirSurface *surface);

    bool blockInput() const;
    void setBlockInput(bool enable);

Q_SIGNALS:
    void blockInputChanged();

private Q_SLOT:
    void setShellSurface(MirSurface *surface);
};

#endif // INPUTFILTERAREA_H
