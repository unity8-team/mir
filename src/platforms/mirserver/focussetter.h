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

#ifndef QPAMIRSERVER_FOCUS_SETTER_H
#define QPAMIRSERVER_FOCUS_SETTER_H

#include <mir/shell/focus_setter.h>

class FocusSetter : public mir::shell::FocusSetter
{
public:
    void set_focus_to(std::shared_ptr<mir::scene::Session> const& new_focus) override;
};

#endif // QPAMIRSERVER_FOCUS_SETTER_H
