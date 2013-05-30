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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_SESSION_DELEGATES_H_
#define UBUNTU_APPLICATION_UI_SESSION_DELEGATES_H_

#include <cstdio>

namespace ubuntu
{
namespace application
{
class LifecycleDelegate : public platform::ReferenceCountedBase
{
public:
    typedef platform::shared_ptr<LifecycleDelegate> Ptr;

    virtual void on_application_resumed() = 0;
    virtual void on_application_about_to_stop() = 0;

protected:
    LifecycleDelegate() {}
    virtual ~LifecycleDelegate() {}

    LifecycleDelegate(const LifecycleDelegate&) = delete;
    LifecycleDelegate& operator=(const LifecycleDelegate&) = delete;
};
}
}

#endif // UBUNTU_APPLICATION_UI_SESSION_DELEGATES_H_
