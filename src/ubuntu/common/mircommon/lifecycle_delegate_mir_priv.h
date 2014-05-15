/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_LIECYCLE_DELEGATE_MIR_PRIV_H_
#define UBUNTU_APPLICATION_LIECYCLE_DELEGATE_MIR_PRIV_H_

#include "mir_toolkit/common.h"

#include <ubuntu/application/lifecycle_delegate.h>

namespace ubuntu
{
namespace application
{
namespace mir
{

class LifecycleDelegate
{
public:
    LifecycleDelegate();
    ~LifecycleDelegate() = default;

    UApplicationLifecycleDelegate* as_u_lifecycle_delegate();
    static LifecycleDelegate* from_u_lifecycle_delegate(UApplicationLifecycleDelegate *u_delegate);

    void ref();
    void unref();
    
    u_on_application_resumed resumed_cb;
    u_on_application_about_to_stop stop_cb;
    void *ctx;

protected:
    LifecycleDelegate(LifecycleDelegate const&) = delete;
    LifecycleDelegate& operator=(LifecycleDelegate const&) = delete;

private:
    unsigned int ref_count;
};

}
}
}

#endif // UBUNTU_APPLICATION_LIECYCLE_DELEGATE_MIR_PRIV_H_
