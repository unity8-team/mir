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

#ifndef FOCUSSETTER_H
#define FOCUSSETTER_H

// mir
#include <mir/shell/focus_setter.h>

// Qt
#include <QObject>

namespace mir
{
namespace shell
{
class InputTargeter;
class Surface;
}
}

class FocusSetter : public QObject, public mir::shell::FocusSetter
{
    Q_OBJECT

public:
    FocusSetter(std::shared_ptr<mir::shell::FocusSetter> const& underlying_setter, std::shared_ptr<mir::shell::InputTargeter> const& keyboard_input_targeter, 
                QObject *parent = 0);
    ~FocusSetter();

    void set_focus_to(std::shared_ptr<mir::shell::Session> const& session) override;
    
    // We support a surface to be given key input when focus is otherwise to be cleared.
    void set_default_keyboard_target(std::weak_ptr<mir::shell::Surface> const& default_keyboard_target);

private:
    std::shared_ptr<mir::shell::FocusSetter> const underlying_setter;
    std::shared_ptr<mir::shell::InputTargeter> const keyboard_input_targeter;
    
    std::weak_ptr<mir::shell::Surface> default_keyboard_target;
};

#endif // FOCUSSETTER_H
