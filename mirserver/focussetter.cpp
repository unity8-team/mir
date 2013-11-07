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

#include <mir/shell/surface.h>

#include "focussetter.h"
#include "logging.h"

namespace msh = mir::shell;

FocusSetter::FocusSetter(std::shared_ptr<msh::FocusSetter> const& underlying_setter, std::shared_ptr<mir::shell::InputTargeter> const& keyboard_input_targeter, QObject *parent)
    :   QObject(parent),
        underlying_setter(underlying_setter),
        keyboard_input_targeter(keyboard_input_targeter)
{
}

FocusSetter::~FocusSetter()
{
}

void FocusSetter::set_focus_to(std::shared_ptr<msh::Session> const& session)
{
    // Ensure we always call the underlying setter to dispatch focus/unfocus notifications.
    underlying_setter->set_focus_to(session);
    if (session == nullptr)
    {
        auto default_target = default_keyboard_target.lock();
        if (!default_target)
            return;
        default_target->take_input_focus(keyboard_input_targeter);
    }
}

void FocusSetter::set_default_keyboard_target(std::weak_ptr<msh::Surface> const& default_target)
{
    auto t = default_target.lock();
    assert(t);

    default_keyboard_target = default_target;
    
    t->take_input_focus(keyboard_input_targeter);
}
