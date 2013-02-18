/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_INPUT_LISTENER_H_
#define UBUNTU_APPLICATION_UI_INPUT_LISTENER_H_

#include "ubuntu/application/ui/input/event.h"
#include "ubuntu/platform/shared_ptr.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
namespace input
{
/** Models a listener for classic input event originating from input devices like mice or touchpads. */
class Listener : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Listener> Ptr;

    /** Invoked whenever a new event is available. */
    virtual void on_new_event(const Event& event) = 0;

protected:
    Listener() {}
    virtual ~Listener() {}

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
};
}
}
}
}

#endif // UBUNTU_APPLICATION_UI_INPUT_LISTENER_H_
