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

#include "lifecycle_delegate_mir_priv.h"

namespace uam = ubuntu::application::mir;

uam::LifecycleDelegate::LifecycleDelegate()
    : resumed_cb(nullptr),
      stop_cb(nullptr),
      ctx(nullptr)
{ 
}

UApplicationLifecycleDelegate* uam::LifecycleDelegate::as_u_lifecycle_delegate()
{
    return static_cast<UApplicationLifecycleDelegate*>(this);
}

uam::LifecycleDelegate* uam::LifecycleDelegate::from_u_lifecycle_delegate(UApplicationLifecycleDelegate *u_delegate)
{
    return static_cast<uam::LifecycleDelegate*>(u_delegate);
}

void uam::LifecycleDelegate::ref()
{
    ref_count++;
}

void uam::LifecycleDelegate::unref()
{
    ref_count--;
    if (ref_count == 0)
        delete this;
}


extern "C"
{

UApplicationLifecycleDelegate*
u_application_lifecycle_delegate_new()
{
    return (new uam::LifecycleDelegate)->as_u_lifecycle_delegate();
}
    
void
u_application_lifecycle_delegate_destroy(UApplicationLifecycleDelegate *u_delegate)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);

    // TODO: What are the intended semantics of this?
    delegate->unref();
}
    
void
u_application_lifecycle_delegate_ref(UApplicationLifecycleDelegate *u_delegate)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    delegate->ref();
}
    
void
u_application_lifecycle_delegate_unref(UApplicationLifecycleDelegate *u_delegate)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    delegate->unref();
}
    
void
u_application_lifecycle_delegate_set_application_resumed_cb(UApplicationLifecycleDelegate *u_delegate,
    u_on_application_resumed cb)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    delegate->resumed_cb = cb;
}
    
u_on_application_resumed
u_application_lifecycle_delegate_get_application_resumed_cb(UApplicationLifecycleDelegate *u_delegate)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    return delegate->resumed_cb;
}
    
void
u_application_lifecycle_delegate_set_application_about_to_stop_cb(UApplicationLifecycleDelegate *u_delegate,
    u_on_application_about_to_stop cb)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    delegate->stop_cb = cb;
}
    
u_on_application_about_to_stop
u_application_lifecycle_delegate_get_application_about_to_stop_cb(UApplicationLifecycleDelegate *u_delegate)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    return delegate->stop_cb;
}
    
void
u_application_lifecycle_delegate_set_context(UApplicationLifecycleDelegate *u_delegate,
    void *context)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    delegate->ctx = context;
}
    
void*
u_application_lifecycle_delegate_get_context(UApplicationLifecycleDelegate *u_delegate,
    void *context)
{
    auto delegate = uam::LifecycleDelegate::from_u_lifecycle_delegate(u_delegate);
    return delegate->ctx;
}

}
